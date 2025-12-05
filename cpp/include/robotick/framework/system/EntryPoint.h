// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(ROBOTICK_PLATFORM_ESP32S3)
#define ROBOTICK_ENTRYPOINT extern "C" void app_main()
#else
#define ROBOTICK_ENTRYPOINT int main()
#endif
