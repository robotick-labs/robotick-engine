#pragma once

#include <stdint.h>

namespace robotick
{
	struct Color
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};

	class Renderer
	{
	  public:
		~Renderer() { cleanup(); }

		// Lifecycle
		void init();
		void clear();
		void present();
		void cleanup();

		// Viewport
		void set_viewport(float w, float h)
		{
			logical_w = w;
			logical_h = h;
		}

		// Drawing
		void draw_ellipse_filled(const float cx, const float cy, const float rx, const float ry, const Color& color);

	  protected:
		[[nodiscard]] int to_px_x(float x) const { return static_cast<int>(x * physical_w / logical_w + 0.5f); }
		[[nodiscard]] int to_px_y(float y) const { return static_cast<int>(y * physical_h / logical_h + 0.5f); }
		[[nodiscard]] int to_px_w(float w) const { return static_cast<int>(w * physical_w / logical_w + 0.5f); }
		[[nodiscard]] int to_px_h(float h) const { return static_cast<int>(h * physical_h / logical_h + 0.5f); }

		int physical_w = 320;
		int physical_h = 240;
		float logical_w = 320.0f;
		float logical_h = 240.0f;
	};
} // namespace robotick
