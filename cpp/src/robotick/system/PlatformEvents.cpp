#include "robotick/framework/system/PlatformEvents.h"

#if defined(ROBOTICK_PLATFORM_DESKTOP)
#include <SDL2/SDL.h>

namespace robotick
{
	static bool s_should_exit = false;

	void poll_platform_events()
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
			{
				s_should_exit = true;
			}
		}
	}

	bool should_exit_application()
	{
		return s_should_exit;
	}
} // namespace robotick

#else

namespace robotick
{
	void poll_platform_events()
	{
		// No-op on platforms without event polling
	}

	bool should_exit_application()
	{
		return false;
	}

} // namespace robotick
#endif
