// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

// robotick/platform/System_desktop.cpp

#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/framework/system/System.h"

namespace robotick
{

	void System::initialize()
	{
		// No-op for now on desktop
		// Could add locale setup, stdout flush tweaks, etc later
	}

} // namespace robotick

#endif // ROBOTICK_PLATFORM_DESKTOP
