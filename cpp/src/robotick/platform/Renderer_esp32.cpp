
#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/platform/Renderer.h"
#include <M5Unified.h>

namespace robotick
{
	static M5Canvas* canvas = nullptr;

	void Renderer::init()
	{
		M5.begin();
		M5.Lcd.setRotation(3);
		physical_w = 320;
		physical_h = 240;
		canvas = new M5Canvas(&M5.Lcd);
		canvas->createSprite(physical_w, physical_h);
	}

	void Renderer::clear()
	{
		canvas->fillScreen(TFT_WHITE);
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

	void Renderer::draw_ellipse_filled(const float cx, const float cy, const float rx, const float ry, const Color& color)
	{
		uint32_t c = canvas->color565(color.r, color.g, color.b);
		canvas->fillEllipse(to_px_x(cx), to_px_y(cy), to_px_w(rx), to_px_h(ry), c);
	}
} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
