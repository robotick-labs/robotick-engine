#define ROBOTICK_PLATFORM_ESP32 // temp - do not commit!

#ifdef ROBOTICK_PLATFORM_ESP32

#include "robotick/api.h"

#include <M5Unified.h>

namespace robotick
{

	struct HeartbeatConfig
	{
		float rest_heart_rate = 60.0f;
	};

	ROBOTICK_BEGIN_FIELDS(HeartbeatConfig)
	ROBOTICK_FIELD(HeartbeatConfig, float, rest_heart_rate)
	ROBOTICK_FIELD(HeartbeatConfig, int, led_gpio)
	ROBOTICK_END_FIELDS()

	struct HeartbeatInputs
	{
		FixedString8 bar1_label;
		float bar1_fraction = 0.0;
		FixedString8 bar2_label;
		float bar2_fraction = 0.0;
		FixedString8 bar3_label;
		float bar3_fraction = 0.0;

		float heart_rate_scale = 1.0f;
	};
	ROBOTICK_BEGIN_FIELDS(HeartbeatInputs)
	ROBOTICK_FIELD(HeartbeatInputs, float, cpu_load)
	ROBOTICK_FIELD(HeartbeatInputs, float, battery_level)
	ROBOTICK_FIELD(HeartbeatInputs, float, wifi_strength)
	ROBOTICK_FIELD(HeartbeatInputs, bool, error_state)
	ROBOTICK_FIELD(HeartbeatInputs, float, attention)
	ROBOTICK_END_FIELDS()

	struct HeartbeatOutputs
	{
		float activation_amount = 1.0f;
	};
	ROBOTICK_BEGIN_FIELDS(HeartbeatOutputs)
	ROBOTICK_FIELD(HeartbeatOutputs, float, activation_amount)
	ROBOTICK_END_FIELDS()

	struct HeartbeatDisplayWorkload
	{
		HeartbeatConfig config;
		HeartbeatInputs inputs;
		HeartbeatOutputs outputs;

		double start_time_sec = 0.0;

		void load() {}

		void setup()
		{
			M5.begin();
			M5.Lcd.setRotation(1);

			start_time_sec = (double)esp_timer_get_time() / 1e6;
		}

		void tick(double delta_time)
		{
			(void)delta_time; // TODO - pass in global time above and use that - so workloads don'fraction need to query globals for common things
							  // like this

			const int64_t now_us = esp_timer_get_time();
			const double now_time_sec = (double)now_us / 1e6;

			const float alive_time_sec = (float)(now_time_sec - start_time_sec);

			float bpm = config.rest_heart_rate;
			if (inputs.error_state)
				bpm = 30.0f;
			else if (inputs.attention > 0.8f)
				bpm = 90.0f;

			const float beat_phase = fmodf(alive_time_sec, 60.0f / bpm) / (60.0f / bpm);
			update_heart(beat_phase);

			// do our drawing:
			M5Canvas canvas(&M5.Lcd);
			canvas.createSprite(320, 240); // Full screen sprite

			draw_heart(canvas, beat_phase);
			draw_stats(canvas, inputs);

			canvas.pushSprite(0, 0); // Copy to screen
		}

		void update_heart(const float beat_phase)
		{
			const float lub_start = 0.00f;
			const float lub_up = 0.075f;
			const float lub_down = 0.20f;

			const float dub_start = 0.275f;
			const float dub_up = 0.06f;
			const float dub_down = 0.1f;

			const float max_activation = 1.0f;
			const float min_activation_hi = 0.7f;
			const float min_activation_lo = 0.5f;

			auto ramp = [](const float fraction) -> float
			{
				// fraction ∈ [0,1], returns smooth pulse: 0 → 1 → 0
				return 0.5f * (1 - cosf(fraction * M_PI));
			};

			outputs.activation_amount = min_activation_lo;

			// Lub pulse
			if (beat_phase >= lub_start && beat_phase < lub_start + lub_up)
			{
				const float fraction = (beat_phase - lub_start) / lub_up;
				outputs.activation_amount = min_activation_lo + (max_activation - min_activation_hi) * ramp(fraction);
			}
			else if (beat_phase >= lub_start + lub_up && beat_phase < lub_start + lub_up + lub_down)
			{
				const float fraction = (beat_phase - lub_start - lub_up) / lub_down;
				outputs.activation_amount = min_activation_hi + (max_activation - min_activation_hi) * (1 - ramp(fraction));
			}
			// Dub pulse
			else if (beat_phase >= dub_start && beat_phase < dub_start + dub_up)
			{
				const float fraction = (beat_phase - dub_start) / dub_up;
				outputs.activation_amount = min_activation_hi + (max_activation - min_activation_hi) * ramp(fraction);
			}
			else if (beat_phase >= dub_start + dub_up && beat_phase < dub_start + dub_up + dub_down)
			{
				const float fraction = (beat_phase - dub_start - dub_up) / dub_down;
				outputs.activation_amount = min_activation_hi + (max_activation - min_activation_hi) * (1 - ramp(fraction));
			}
			else
			{
				const float settle_start = dub_start + dub_up + dub_down;
				float settle_duration = 1.0f - settle_start;
				const float fraction = (beat_phase - settle_start) / settle_duration;
				fraction = std::clamp(fraction, 0.0f, 1.0f);

				outputs.activation_amount = min_activation_lo + (min_activation_hi - min_activation_lo) * (1.0f - fraction);
			}
		}

		void draw_heart(M5Canvas& canvas, const float beat_phase)
		{
			const float brightness = outputs.activation_amount;

			const int cx = 160, cy = 120;
			const int radius = static_cast<int>(75 + 15 * brightness);

			const float scale_colour = 0.2f;

			const uint16_t color = canvas.color565(static_cast<uint8_t>(255.f * (1.f - scale_colour + scale_colour * brightness)), 0, 0);

			canvas.fillCircle(cx, cy, radius, color);
		}

		void draw_stats(M5Canvas& canvas, const HeartbeatInputs& in)
		{
			int x = 60;
			int y = 190;
			int h = 10;
			int w = 100;
			int label_x = 10;
			int spacing = 15;

			canvas.setTextSize(1);
			canvas.setTextColor(WHITE);

			// CPU
			canvas.drawString("CPU", label_x, y);
			canvas.drawRect(x, y, w, h, DARKGREY);
			canvas.fillRect(x, y, static_cast<int>(in.cpu_load * w), h, GREEN);
			y += spacing;

			// Batteru
			canvas.drawString("BAT", label_x, y);
			canvas.drawRect(x, y, w, h, DARKGREY);
			canvas.fillRect(x, y, static_cast<int>(in.battery_level * w), h, YELLOW);
			y += spacing;

			// Wifi
			canvas.drawString("WIFI", label_x, y);
			canvas.drawRect(x, y, w, h, DARKGREY);
			canvas.fillRect(x, y, static_cast<int>(in.wifi_strength * w), h, BLUE);
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(HeartbeatDisplayWorkload, HeartbeatConfig, HeartbeatInputs, HeartbeatOutputs)

} // namespace robotick

#endif