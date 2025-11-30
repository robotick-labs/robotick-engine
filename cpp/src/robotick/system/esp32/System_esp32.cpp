// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

// robotick/platform/System_esp32.cpp

#if defined(ROBOTICK_PLATFORM_ESP32S3)

#include "robotick/framework/system/System.h"

namespace robotick
{

	void System::initialize()
	{
		// No-op: board-specific initialization is expected to be handled
		// by higher-level helpers (e.g., robotick::boards::m5::ensure_initialized()).
	}

} // namespace robotick

#endif // ROBOTICK_PLATFORM_ESP32S3
