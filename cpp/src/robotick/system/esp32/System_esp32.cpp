// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

// robotick/platform/System_esp32.cpp

#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/framework/system/System.h"
#include <M5Unified.h>

namespace robotick
{

	void System::initialize()
	{
		M5.begin();
		// Add more platform-specific init here as needed
	}

} // namespace robotick

#endif // ROBOTICK_PLATFORM_ESP32
