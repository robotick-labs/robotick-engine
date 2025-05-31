// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Detect core platform type
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
#if defined(__ANDROID__)
#define ROBOTICK_PLATFORM_MOBILE
#else
#define ROBOTICK_PLATFORM_DESKTOP
#endif
#else
#define ROBOTICK_PLATFORM_EMBEDDED
#endif

// Named embedded platform overrides (ESP32, STM32, etc.)
#if defined(ESP_PLATFORM)
#define ROBOTICK_PLATFORM_ESP32
#endif

// Extendable for more explicit platforms later
// #define ROBOTICK_PLATFORM_STM32
// #define ROBOTICK_PLATFORM_PI
