
#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/platform/Renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>

namespace robotick
{
	static SDL_Window* window = nullptr;
	static SDL_Renderer* renderer = nullptr;

	void Renderer::init()
	{
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

		if (SDL_Init(SDL_INIT_VIDEO) != 0)
			ROBOTICK_FATAL_EXIT("SDL_Init failed: %s", SDL_GetError());

		SDL_DisplayMode display_mode;
		if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0)
			ROBOTICK_FATAL_EXIT("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());

		window =
			SDL_CreateWindow("FaceDisplay", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, display_mode.w, display_mode.h, SDL_WINDOW_FULLSCREEN);

		if (!window)
			ROBOTICK_FATAL_EXIT("SDL_CreateWindow failed: %s", SDL_GetError());

		SDL_ShowWindow(window);
		SDL_RaiseWindow(window);

		SDL_GetWindowSize(window, &physical_w, &physical_h);
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		if (!renderer)
			ROBOTICK_FATAL_EXIT("SDL_CreateRenderer failed: %s", SDL_GetError());

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
	}

	void Renderer::cleanup()
	{
		if (renderer)
		{
			SDL_DestroyRenderer(renderer);
			renderer = nullptr;
		}

		if (window)
		{
			SDL_DestroyWindow(window);
			window = nullptr;
		}

		SDL_Quit();
	}

	void Renderer::clear()
	{
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);
	}

	void Renderer::present()
	{
		SDL_RenderPresent(renderer);
	}

	void Renderer::draw_ellipse_filled(const float cx, const float cy, const float rx, const float ry, const Color& color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

		const int cx_px = to_px_x(cx);
		const int cy_px = to_px_y(cy);
		const int rx_px = to_px_w(rx);
		const int ry_px = to_px_h(ry);

		filledEllipseRGBA(renderer, cx_px, cy_px, rx_px, ry_px, color.r, color.g, color.b, color.a);
	}
} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
