// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/platform/NetworkManager.h"
#include "robotick/api.h"

#if defined(ROBOTICK_PLATFORM_ESP32)

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

namespace robotick
{

	bool NetworkClient::connect(const NetworkClientConfig& cfg)
	{
		if (cfg.type != NetworkType::Wifi)
			return false;

		static const char* TAG = "NetworkClient";

		// Init NVS
		esp_err_t ret = nvs_flash_init();
		if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
		{
			ESP_ERROR_CHECK(nvs_flash_erase());
			ESP_ERROR_CHECK(nvs_flash_init());
		}

		// Init netif, events, WiFi
		ESP_ERROR_CHECK(esp_netif_init());
		ESP_ERROR_CHECK(esp_event_loop_create_default());
		esp_netif_t* netif = esp_netif_create_default_wifi_sta();

		wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

		wifi_config_t wifi_cfg = {};
		strncpy((char*)wifi_cfg.sta.ssid, cfg.ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
		strncpy((char*)wifi_cfg.sta.password, cfg.password.c_str(), sizeof(wifi_cfg.sta.password));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

		ESP_ERROR_CHECK(esp_wifi_start());
		ESP_LOGI(TAG, "Connecting to SSID: %s", cfg.ssid.c_str());

		ESP_ERROR_CHECK(esp_wifi_connect());

		// Wait for IP assignment
		EventGroupHandle_t wifi_event_group = xEventGroupCreate();
		static constexpr int CONNECTED_BIT = BIT0;

		// Lambda to capture events
		auto wifi_event_handler = [](void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
		{
			if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
			{
				xEventGroupSetBits((EventGroupHandle_t)arg, CONNECTED_BIT);
			}
		};

		ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, wifi_event_group, nullptr));

		// Wait for IP assignment or timeout
		constexpr int timeout_ms = 10000;
		EventBits_t bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
		vEventGroupDelete(wifi_event_group);

		if (bits & CONNECTED_BIT)
		{
			esp_netif_ip_info_t ip_info;
			ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
			ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&ip_info.ip));
			return true;
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

#include <cstdio>
#include <cstdlib>

namespace robotick
{

	bool NetworkHotspot::start(const NetworkHotspotConfig& cfg)
	{
		if (cfg.type != NetworkType::Wifi)
		{
			return false;
		}

		FixedString256 cmd;
		cmd.format("nmcli dev wifi hotspot ifname %s ssid %s password %s && ip a | grep %s",
			cfg.iface.c_str(),
			cfg.ssid.c_str(),
			cfg.password.c_str(),
			cfg.iface.c_str());

		const int result = std::system(cmd.c_str());
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

		FixedString256 cmd;
		cmd.format("nmcli dev wifi connect '%s' password '%s' ifname %s", cfg.ssid.c_str(), cfg.password.c_str(), cfg.iface.c_str());

		const int result = std::system(cmd.c_str());
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
		return std::system("nmcli -t -f WIFI g | grep -q enabled") == 0;
	}

} // namespace robotick

#else

namespace robotick
{

	bool NetworkHotspot::start(const NetworkHotspotConfig&)
	{
		return false;
	}

} // namespace robotick

#endif
