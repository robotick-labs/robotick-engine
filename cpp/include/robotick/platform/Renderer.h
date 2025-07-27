#pragma once

#include "robotick/framework/math/Vec2.h"

#include <stdint.h>

namespace robotick
{
	struct Color
	{
		uint8_t r = 0, g = 0, b = 0, a = 255;
	};

	namespace Colors
	{
		inline constexpr Color Black = {0, 0, 0, 255};
		inline constexpr Color White = {255, 255, 255, 255};
		inline constexpr Color Red = {255, 0, 0, 255};
		inline constexpr Color Green = {0, 255, 0, 255};
		inline constexpr Color Blue = {0, 0, 255, 255};
		inline constexpr Color Yellow = {255, 255, 0, 255};
		inline constexpr Color Orange = {255, 165, 0, 255};
	} // namespace Colors

	enum class TextAlign
	{
		TopLeft,
		Center,
	};

	class Renderer
	{
	  public:
		~Renderer() { cleanup(); }

		// Lifecycle
		void init();
		void clear(const Color& color = Colors::Black);
		void present();
		void cleanup();

		// Viewport
		void set_viewport(float w, float h)
		{
			logical_w = w;
			logical_h = h;
		}

		// Drawing
		void draw_ellipse_filled(const Vec2& center, const float rx, const float ry, const Color& color);
		void draw_circle_filled(const Vec2& center, const float radius, const Color& color) { draw_ellipse_filled(center, radius, radius, color); }
		void draw_triangle_filled(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Color& color);
		void draw_text(const char* text, const Vec2& pos, const float size, const TextAlign align, const Color& color);

	  protected:
		void update_scale()
		{
			const float scale_x = static_cast<float>(physical_w) / logical_w;
			const float scale_y = static_cast<float>(physical_h) / logical_h;
			scale = std::min(scale_x, scale_y);

			offset_x = (physical_w - static_cast<int>(logical_w * scale)) / 2;
			offset_y = (physical_h - static_cast<int>(logical_h * scale)) / 2;
		}

		[[nodiscard]] int to_px_x(float x) const { return static_cast<int>(x * scale + offset_x + 0.5f); }
		[[nodiscard]] int to_px_y(float y) const { return static_cast<int>(y * scale + offset_y + 0.5f); }
		[[nodiscard]] int to_px_w(float w) const { return static_cast<int>(w * scale + 0.5f); }
		[[nodiscard]] int to_px_h(float h) const { return static_cast<int>(h * scale + 0.5f); }

		int physical_w = 320;
		int physical_h = 240;
		float logical_w = 320.0f;
		float logical_h = 240.0f;

		float scale = 1.0f;
		int offset_x = 0;
		int offset_y = 0;
	};
} // namespace robotick
