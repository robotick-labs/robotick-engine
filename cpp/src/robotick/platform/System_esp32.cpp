#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/platform/System.h"
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