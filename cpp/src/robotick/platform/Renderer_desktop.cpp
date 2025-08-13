#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/platform/PlatformEvents.h"
#include "robotick/platform/Renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>

namespace robotick
{
	static SDL_Window* window = nullptr;
	static SDL_Renderer* renderer = nullptr;
	static TTF_Font* font = nullptr;
	static int current_font_size = 0;

	bool is_windowed_mode()
	{
		const char* env = std::getenv("ROBOTICK_WINDOWED");
		return env && std::atoi(env) != 0;
	}

	void Renderer::init()
	{
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

		if (SDL_Init(SDL_INIT_VIDEO) != 0)
			ROBOTICK_FATAL_EXIT("SDL_Init failed: %s", SDL_GetError());

		if (TTF_Init() != 0)
			ROBOTICK_FATAL_EXIT("TTF_Init failed: %s", TTF_GetError());

		SDL_DisplayMode display_mode;
		if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0)
			ROBOTICK_FATAL_EXIT("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());

		const bool is_windowed = is_windowed_mode();
		const uint32_t window_flags = is_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
		const int width = is_windowed ? display_mode.w / 4 : display_mode.w;
		const int height = is_windowed ? display_mode.h / 4 : display_mode.w;

		window = SDL_CreateWindow("Robotick Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
		if (!window)
			ROBOTICK_FATAL_EXIT("SDL_CreateWindow failed: %s", SDL_GetError());

		SDL_ShowWindow(window);
		SDL_RaiseWindow(window);

		SDL_GetWindowSize(window, &physical_w, &physical_h);

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
		if (!renderer)
			ROBOTICK_FATAL_EXIT("SDL_CreateRenderer failed: %s", SDL_GetError());

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);

		update_scale();
	}

	void Renderer::cleanup()
	{
		if (font)
		{
			TTF_CloseFont(font);
			font = nullptr;
			current_font_size = 0;
		}

		TTF_Quit();

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

	void Renderer::clear(const Color& color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
		SDL_RenderClear(renderer);
	}

	void Renderer::present()
	{
		SDL_RenderPresent(renderer);

		poll_platform_events();
	}

	void Renderer::draw_ellipse_filled(const Vec2& center, const float rx, const float ry, const Color& color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

		const int cx_px = to_px_x(center.x);
		const int cy_px = to_px_y(center.y);
		const int rx_px = to_px_w(rx);
		const int ry_px = to_px_h(ry);

		filledEllipseRGBA(renderer, cx_px, cy_px, rx_px, ry_px, color.r, color.g, color.b, color.a);
	}

	void Renderer::draw_triangle_filled(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Color& color)
	{
		const int x0 = to_px_x(p0.x);
		const int y0 = to_px_y(p0.y);
		const int x1 = to_px_x(p1.x);
		const int y1 = to_px_y(p1.y);
		const int x2 = to_px_x(p2.x);
		const int y2 = to_px_y(p2.y);

		filledTrigonRGBA(renderer, x0, y0, x1, y1, x2, y2, color.r, color.g, color.b, color.a);
	}

	void Renderer::draw_text(const char* text, const Vec2& pos, const float size, const TextAlign align, const Color& color)
	{
		if (!text || !*text || !renderer)
			return;

		const int font_size = static_cast<int>(size * scale);
		if (!font || current_font_size != font_size)
		{
			if (font)
			{
				TTF_CloseFont(font);
				font = nullptr;
			}

			// Try default font path; make this configurable in future
#if defined(__linux__)
			const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#elif defined(_WIN32)
			const char* font_path = "C:\\Windows\\Fonts\\arial.ttf";
#else
			const char* font_path = "/System/Library/Fonts/Supplemental/Arial.ttf"; // macOS
#endif

			font = TTF_OpenFont(font_path, font_size);
			if (!font)
			{
				ROBOTICK_WARNING("Failed to load font at '%s': %s", font_path, TTF_GetError());
				return;
			}
			current_font_size = font_size;
		}

		SDL_Color sdl_color = {color.r, color.g, color.b, color.a};
		SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, sdl_color);
		if (!surface)
			return;

		SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
		if (!texture)
		{
			SDL_FreeSurface(surface);
			return;
		}

		SDL_Rect dst;
		dst.w = surface->w;
		dst.h = surface->h;

		const int px = to_px_x(pos.x);
		const int py = to_px_y(pos.y);

		switch (align)
		{
		case TextAlign::TopLeft:
			dst.x = px;
			dst.y = py;
			break;
		case TextAlign::Center:
			dst.x = px - dst.w / 2;
			dst.y = py - dst.h / 2;
			break;
		default:
			dst.x = px;
			dst.y = py;
			break;
		}

		SDL_RenderCopy(renderer, texture, nullptr, &dst);
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(surface);
	}

} // namespace robotick

#endif // ROBOTICK_PLATFORM_DESKTOP
