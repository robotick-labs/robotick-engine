// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace robotick {

enum class NetworkType {
    Wifi,
    Ethernet  // Reserved for future use
};

struct NetworkHotspotConfig {
    NetworkType type = NetworkType::Wifi;
    std::string iface = "wlan0";
    std::string ssid = "robotick-demo";
    std::string password = "letmein123";
};

struct NetworkClientConfig {
    NetworkType type = NetworkType::Wifi;
    std::string iface = "wlan0";
    std::string ssid = "robotick-demo";
    std::string password = "letmein123";
};

class NetworkHotspot {
public:
    static bool start(const NetworkHotspotConfig& cfg);
    // the hotspot with auto-terminate with our application process
};

class NetworkClient {
public:
    static bool connect(const NetworkClientConfig& cfg);
    static bool is_connected();
};

} // namespace robotick
