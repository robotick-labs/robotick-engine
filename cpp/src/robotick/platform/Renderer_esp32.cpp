#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/platform/Renderer.h"
#include <M5Unified.h>

namespace robotick
{
	static M5Canvas* canvas = nullptr;

	void Renderer::init()
	{
		M5.Lcd.setRotation(3);
		physical_w = 320;
		physical_h = 240;
		canvas = new M5Canvas(&M5.Lcd);
		canvas->createSprite(physical_w, physical_h);
	}

	void Renderer::clear(const Color& color)
	{
		canvas->fillScreen(canvas->color565(color.r, color.g, color.b));
	}

	void Renderer::present()
	{
		canvas->pushSprite(0, 0);
	}

	void Renderer::cleanup()
	{
		if (canvas)
		{
			delete canvas;
			canvas = nullptr;
		}
	}

	void Renderer::draw_ellipse_filled(const Vec2& center, const float rx, const float ry, const Color& color)
	{
		canvas->setColor(color.r, color.g, color.b);
		canvas->fillEllipse(to_px_x(center.x), to_px_y(center.y), to_px_w(rx), to_px_h(ry));
	}

	void Renderer::draw_triangle_filled(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Color& color)
	{
		int x0 = to_px_x(p0.x);
		int y0 = to_px_y(p0.y);
		int x1 = to_px_x(p1.x);
		int y1 = to_px_y(p1.y);
		int x2 = to_px_x(p2.x);
		int y2 = to_px_y(p2.y);

		uint32_t c = canvas->color565(color.r, color.g, color.b);
		canvas->fillTriangle(x0, y0, x1, y1, x2, y2, c);
	}

	void Renderer::draw_text(const char* text, const Vec2& pos, const float size, const TextAlign align, const Color& color)
	{
		if (!text || !*text || !canvas)
			return;

		canvas->setTextSize(1);
		canvas->setTextColor(canvas->color565(color.r, color.g, color.b));
		canvas->setTextDatum(align == TextAlign::Center ? middle_center : top_left);

		canvas->drawString(text, to_px_x(pos.x), to_px_y(pos.y));
	}
} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
