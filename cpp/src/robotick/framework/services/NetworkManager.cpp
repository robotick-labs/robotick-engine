// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/services/NetworkManager.h"
#include "robotick/api.h"

#if defined(ROBOTICK_PLATFORM_ESP32S3)

#include <lwip/ip4_addr.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

namespace robotick
{
	namespace
	{
		struct WifiConnectWaitContext
		{
			EventGroupHandle_t event_group = nullptr;
			const char* ssid = nullptr;
		};

		void esp32_check_ok_or_reuse(const esp_err_t err, const char* what)
		{
			if (err == ESP_OK)
			{
				return;
			}

			if (err == ESP_ERR_INVALID_STATE)
			{
				ESP_LOGI("NetworkClient", "%s already initialized; reusing existing state", what);
				return;
			}

			ESP_ERROR_CHECK(err);
		}
	} // namespace

	bool NetworkClient::connect(const NetworkClientConfig& cfg)
	{
		if (cfg.type != NetworkType::Wifi)
			return false;

		static const char* TAG = "NetworkClient";
		const bool use_static_ipv4 = !cfg.static_ipv4.empty() || !cfg.gateway_ipv4.empty() || !cfg.netmask_ipv4.empty();

		// Init NVS
		esp_err_t ret = nvs_flash_init();
		if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
		{
			ESP_ERROR_CHECK(nvs_flash_erase());
			ESP_ERROR_CHECK(nvs_flash_init());
		}

		// Init netif, events, WiFi
		esp32_check_ok_or_reuse(esp_netif_init(), "esp_netif");
		esp32_check_ok_or_reuse(esp_event_loop_create_default(), "esp_event_loop");

		esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
		if (netif == nullptr)
		{
			netif = esp_netif_create_default_wifi_sta();
		}
		ROBOTICK_ASSERT_MSG(netif != nullptr, "Failed to initialize ESP32 STA netif");

		wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
		esp32_check_ok_or_reuse(esp_wifi_init(&wifi_init_cfg), "esp_wifi");

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

		wifi_config_t wifi_cfg = {};
		strncpy((char*)wifi_cfg.sta.ssid, cfg.ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
		strncpy((char*)wifi_cfg.sta.password, cfg.password.c_str(), sizeof(wifi_cfg.sta.password));
		// NetworkManager/NM hotspots typically advertise WPA-PSK compatibility rather
		// than a strict WPA2-only mode, so keep the auth threshold permissive enough
		// for Pi-hosted robot hotspots.
		wifi_cfg.sta.threshold.authmode = cfg.password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_PSK;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

		if (use_static_ipv4)
		{
			esp_netif_ip_info_t ip_info = {};
			ip4_addr_t parsed_ip = {};
			ip4_addr_t parsed_gw = {};
			ip4_addr_t parsed_netmask = {};
			if (!ip4addr_aton(cfg.static_ipv4.c_str(), &parsed_ip))
			{
				ESP_LOGW(TAG, "Invalid static IP: %s", cfg.static_ipv4.c_str());
				return false;
			}
			if (!ip4addr_aton(cfg.gateway_ipv4.c_str(), &parsed_gw))
			{
				ESP_LOGW(TAG, "Invalid gateway IP: %s", cfg.gateway_ipv4.c_str());
				return false;
			}
			if (!ip4addr_aton(cfg.netmask_ipv4.c_str(), &parsed_netmask))
			{
				ESP_LOGW(TAG, "Invalid netmask IP: %s", cfg.netmask_ipv4.c_str());
				return false;
			}

			ip_info.ip.addr = parsed_ip.addr;
			ip_info.gw.addr = parsed_gw.addr;
			ip_info.netmask.addr = parsed_netmask.addr;

			ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
			ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
		}

		EventGroupHandle_t wifi_event_group = xEventGroupCreate();
		WifiConnectWaitContext wait_context{};
		wait_context.event_group = wifi_event_group;
		wait_context.ssid = cfg.ssid.c_str();
		static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
		static constexpr EventBits_t GOT_IP_BIT = BIT1;
		static constexpr EventBits_t FAILED_BIT = BIT2;

		// Register handlers before connect so static-IP clients do not miss the
		// association event while DHCP is disabled.
		auto wifi_event_handler = [](void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
		{
			auto* wait_context = static_cast<WifiConnectWaitContext*>(arg);
			EventGroupHandle_t event_group = wait_context->event_group;

			if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
			{
				xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
				return;
			}

			if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
			{
				const auto* disconnect = static_cast<const wifi_event_sta_disconnected_t*>(event_data);
				ESP_LOGW(
					TAG,
					"Disconnected from SSID '%s' (reason=%d)",
					wait_context->ssid ? wait_context->ssid : "<unknown>",
					disconnect ? static_cast<int>(disconnect->reason) : -1);
				xEventGroupSetBits(event_group, FAILED_BIT);
				return;
			}

			if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
			{
				xEventGroupSetBits(event_group, GOT_IP_BIT);
			}
		};

		esp_event_handler_instance_t wifi_connected_instance = nullptr;
		esp_event_handler_instance_t wifi_disconnected_instance = nullptr;
		esp_event_handler_instance_t got_ip_instance = nullptr;
		ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, wifi_event_handler, &wait_context, &wifi_connected_instance));
		ESP_ERROR_CHECK(esp_event_handler_instance_register(
			WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, &wait_context, &wifi_disconnected_instance));
		ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, &wait_context, &got_ip_instance));

		ESP_ERROR_CHECK(esp_wifi_start());
		ESP_LOGI(TAG, "Connecting to SSID: %s", cfg.ssid.c_str());

		ESP_ERROR_CHECK(esp_wifi_connect());

		// Static-IP clients still need the Wi-Fi association event, while DHCP-backed
		// clients wait for the usual got-IP event.
		constexpr int timeout_ms = 10000;
		const EventBits_t success_bit = use_static_ipv4 ? WIFI_CONNECTED_BIT : GOT_IP_BIT;
		EventBits_t bits = xEventGroupWaitBits(wifi_event_group, success_bit | FAILED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, wifi_connected_instance));
		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_disconnected_instance));
		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_instance));
		vEventGroupDelete(wifi_event_group);

		if (bits & success_bit)
		{
			esp_netif_ip_info_t ip_info;
			ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
			ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&ip_info.ip));
			return true;
		}

		if (bits & FAILED_BIT)
		{
			ESP_LOGW(TAG, "Connection to SSID '%s' failed before IP assignment.", cfg.ssid.c_str());
			return false;
		}

		else
		{
			ESP_LOGW(TAG, "Connection to SSID '%s' timed out.", cfg.ssid.c_str());
			return false;
		}
	}

	bool NetworkClient::is_connected()
	{
		wifi_ap_record_t ap_info;
		return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
	}

	bool NetworkHotspot::start(const NetworkHotspotConfig&)
	{
		return false; // Not supported on ESP32
	}

} // namespace robotick

#elif defined(ROBOTICK_PLATFORM_LINUX)

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace robotick
{

	namespace
	{
		bool is_valid_iface_name(const char* iface)
		{
			if (!iface || iface[0] == '\0')
			{
				return false;
			}

			for (size_t i = 0; iface[i] != '\0'; ++i)
			{
				const unsigned char c = static_cast<unsigned char>(iface[i]);
				if (!::isalnum(c) && c != '-' && c != '_' && c != '.' && c != ':')
				{
					return false;
				}
			}

			return true;
		}

		bool is_valid_nmcli_text(const char* value)
		{
			if (!value || value[0] == '\0')
			{
				return false;
			}

			for (size_t i = 0; value[i] != '\0'; ++i)
			{
				const char c = value[i];
				if (c == '\n' || c == '\r')
				{
					return false;
				}
			}

			return true;
		}

		FixedString256 shell_escape_single_quotes(const char* value)
		{
			FixedString256 escaped;
			if (!value)
			{
				return escaped;
			}

			for (size_t i = 0; value[i] != '\0'; ++i)
			{
				const char c = value[i];
				if (c == '\'')
				{
					escaped.append("'\\''");
				}
				else
				{
					char buffer[2] = {c, '\0'};
					escaped.append(buffer);
				}
			}

			return escaped;
		}
	} // namespace

	bool NetworkHotspot::start(const NetworkHotspotConfig& cfg)
	{
		if (cfg.type != NetworkType::Wifi)
		{
			return false;
		}

		if (!is_valid_iface_name(cfg.iface.c_str()))
		{
			ROBOTICK_WARNING("NetworkHotspot invalid interface: %s", cfg.iface.c_str());
			return false;
		}

		if (!is_valid_nmcli_text(cfg.ssid.c_str()))
		{
			ROBOTICK_WARNING("NetworkHotspot invalid SSID");
			return false;
		}

		if (!is_valid_nmcli_text(cfg.connection_name.c_str()))
		{
			ROBOTICK_WARNING("NetworkHotspot invalid connection name");
			return false;
		}

		if (!is_valid_nmcli_text(cfg.password.c_str()))
		{
			ROBOTICK_WARNING("NetworkHotspot invalid password");
			return false;
		}

		if (!is_valid_nmcli_text(cfg.ipv4_address_cidr.c_str()))
		{
			ROBOTICK_WARNING("NetworkHotspot invalid IPv4 CIDR");
			return false;
		}

		const FixedString256 escaped_connection_name = shell_escape_single_quotes(cfg.connection_name.c_str());
		const FixedString256 escaped_ssid = shell_escape_single_quotes(cfg.ssid.c_str());
		const FixedString256 escaped_password = shell_escape_single_quotes(cfg.password.c_str());
		const FixedString256 escaped_ipv4_cidr = shell_escape_single_quotes(cfg.ipv4_address_cidr.c_str());

		const char* privilege_prefix = geteuid() == 0 ? "" : "sudo -n ";

		FixedString512 cmd;
		cmd.format(
			"%snmcli connection delete '%s' >/dev/null 2>&1 || true; "
			"%snmcli connection add type wifi ifname %s con-name '%s' autoconnect no ssid '%s' >/dev/null && "
			"%snmcli connection modify '%s' 802-11-wireless.mode ap 802-11-wireless.band bg "
			"ipv4.method shared ipv4.addresses '%s' ipv6.method disabled "
			"wifi-sec.key-mgmt wpa-psk wifi-sec.proto rsn wifi-sec.psk '%s' && "
			"%snmcli connection up '%s'",
			privilege_prefix,
			escaped_connection_name.c_str(),
			privilege_prefix,
			cfg.iface.c_str(),
			escaped_connection_name.c_str(),
			escaped_ssid.c_str(),
			privilege_prefix,
			escaped_connection_name.c_str(),
			escaped_ipv4_cidr.c_str(),
			escaped_password.c_str(),
			privilege_prefix,
			escaped_connection_name.c_str());

		const int result = std_approved::system(cmd.c_str());
		const bool success = result == 0;
		if (success)
		{
			ROBOTICK_INFO("NetworkHotspot successfully started using: %s", cmd.c_str());
		}
		else
		{
			ROBOTICK_WARNING("NetworkHotspot failed to start using: %s", cmd.c_str());
		}

		return success;
	}

	bool NetworkClient::connect(const NetworkClientConfig& cfg)
	{
		if (cfg.type != NetworkType::Wifi)
		{
			return false;
		}

		if (!is_valid_iface_name(cfg.iface.c_str()))
		{
			ROBOTICK_WARNING("NetworkClient invalid interface: %s", cfg.iface.c_str());
			return false;
		}

		if (!is_valid_nmcli_text(cfg.ssid.c_str()))
		{
			ROBOTICK_WARNING("NetworkClient invalid SSID");
			return false;
		}

		if (!is_valid_nmcli_text(cfg.password.c_str()))
		{
			ROBOTICK_WARNING("NetworkClient invalid password");
			return false;
		}

		const FixedString256 escaped_ssid = shell_escape_single_quotes(cfg.ssid.c_str());
		const FixedString256 escaped_password = shell_escape_single_quotes(cfg.password.c_str());

		FixedString256 cmd;
		cmd.format("nmcli dev wifi connect '%s' password '%s' ifname %s", escaped_ssid.c_str(), escaped_password.c_str(), cfg.iface.c_str());

		const int result = std_approved::system(cmd.c_str());
		const bool success = result == 0;
		if (success)
		{
			ROBOTICK_INFO("NetworkClient successfully connected using: %s", cmd.c_str());
		}
		else
		{
			ROBOTICK_WARNING("NetworkClient failed to connect using: %s", cmd.c_str());
		}

		return success;
	}

	bool NetworkClient::is_connected()
	{
		return std_approved::system("nmcli -t -f STATE g | grep -q '^connected$'") == 0;
	}

} // namespace robotick

#else

namespace robotick
{

	bool NetworkHotspot::start(const NetworkHotspotConfig&)
	{
		return false;
	}

	bool NetworkClient::connect(const NetworkClientConfig&)
	{
		return false;
	}

	bool NetworkClient::is_connected()
	{
		return false;
	}

} // namespace robotick

#endif
