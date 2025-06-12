// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/platform/NetworkManager.h"

#if defined(ROBOTICK_PLATFORM_ESP32)

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>

namespace robotick {

bool NetworkClient::connect(const NetworkClientConfig& cfg) {
    if (cfg.type != NetworkType::Wifi) return false;

    static const char* TAG = "NetworkClient";
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_cfg);

    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wifi_cfg = {};
    strncpy((char*)wifi_cfg.sta.ssid, cfg.ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, cfg.password.c_str(), sizeof(wifi_cfg.sta.password));

    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "Connecting to SSID: %s", cfg.ssid.c_str());
    return true;
}

bool NetworkClient::is_connected() {
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

bool NetworkHotspot::start(const NetworkHotspotConfig&) {
    return false; // Not supported on ESP32
}

bool NetworkHotspot::stop() {
    return false;
}

} // namespace robotick

#elif defined(ROBOTICK_PLATFORM_UBUNTU)

#include <cstdlib>
#include <cstdio>

namespace robotick {

bool NetworkHotspot::start(const NetworkHotspotConfig& cfg) {
    if (cfg.type != NetworkType::Wifi) return false;
    std::string cmd = "nmcli dev wifi hotspot ifname " + cfg.iface +
                      " ssid " + cfg.ssid + " password " + cfg.password;
    return std::system(cmd.c_str()) == 0;
}

bool NetworkHotspot::stop() {
    return std::system("nmcli connection down Hotspot") == 0;
}

bool NetworkClient::connect(const NetworkClientConfig& cfg) {
    if (cfg.type != NetworkType::Wifi) return false;
    std::string cmd = "nmcli dev wifi connect '" + cfg.ssid +
                      "' password '" + cfg.password + "' ifname " + cfg.iface;
    return std::system(cmd.c_str()) == 0;
}

bool NetworkClient::is_connected() {
    return std::system("nmcli -t -f WIFI g | grep -q enabled") == 0;
}

} // namespace robotick

#else

namespace robotick {

bool NetworkHotspot::start(const NetworkHotspotConfig&) { return false; }
bool NetworkHotspot::stop() { return false; }
bool NetworkClient::connect(const NetworkClientConfig&) { return false; }
bool NetworkClient::is_connected() { return false; }

} // namespace robotick

#endif
