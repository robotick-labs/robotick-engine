// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#if defined(ROBOTICK_PLATFORM_ESP32)
#include <M5Unified.h>
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)

namespace robotick
{

	struct HeartbeatConfig
	{
		float rest_heart_rate = 60.0f;
	};

	ROBOTICK_REGISTER_STRUCT_BEGIN(HeartbeatConfig)
	ROBOTICK_STRUCT_FIELD(HeartbeatConfig, float, rest_heart_rate)
	ROBOTICK_REGISTER_STRUCT_END(HeartbeatConfig)

	struct HeartbeatInputs
	{
		FixedString8 bar1_label = "Ha";
		float bar1_fraction = 0.9;
		FixedString8 bar2_label = "Ti";
		float bar2_fraction = 0.0;
		FixedString8 bar3_label = "Ex";
		float bar3_fraction = 0.6;
		FixedString8 bar4_label = "Hu";
		float bar4_fraction = 0.2;

		float heart_rate_scale = 1.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(HeartbeatInputs)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, FixedString8, bar1_label)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, float, bar1_fraction)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, FixedString8, bar2_label)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, float, bar2_fraction)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, FixedString8, bar3_label)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, float, bar3_fraction)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, FixedString8, bar4_label)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, float, bar4_fraction)
	ROBOTICK_STRUCT_FIELD(HeartbeatInputs, float, heart_rate_scale)
	ROBOTICK_REGISTER_STRUCT_END(HeartbeatInputs)

	struct HeartbeatOutputs
	{
		float activation_amount = 1.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(HeartbeatOutputs)
	ROBOTICK_STRUCT_FIELD(HeartbeatOutputs, float, activation_amount)
	ROBOTICK_REGISTER_STRUCT_END(HeartbeatOutputs)

	struct HeartbeatDisplayWorkload
	{
		HeartbeatConfig config;
		HeartbeatInputs inputs;
		HeartbeatOutputs outputs;

		float start_time_sec = 0.0f;

#if defined(ROBOTICK_PLATFORM_ESP32)

		void setup()
		{
			M5.begin();
			M5.Lcd.setRotation(1);

			start_time_sec = (float)esp_timer_get_time() / 1e6;
		}

		void tick(const TickInfo& tick_info)
		{
			const float alive_time_sec = static_cast<float>(tick_info.time_now_ns * 1e-9); // avoids any float/float

			const float bpm = config.rest_heart_rate * inputs.heart_rate_scale;
			const float beat_duration = 60.0f / bpm;
			const float beat_phase = fmodf(alive_time_sec, beat_duration) / beat_duration;

			update_heart(beat_phase);

			M5Canvas canvas(&M5.Lcd);
			canvas.createSprite(320, 240);

			draw_heart(canvas, 1.0f);
			draw_stats(canvas, inputs);

			canvas.pushSprite(0, 0);
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
				return 0.5f * (1 - cosf(fraction * M_PI));
			};

			outputs.activation_amount = min_activation_lo;

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
				float fraction = (beat_phase - settle_start) / settle_duration;
				fraction = std::clamp(fraction, 0.0f, 1.0f);

				outputs.activation_amount = min_activation_lo + (min_activation_hi - min_activation_lo) * (1.0f - fraction);
			}
		}

		void draw_heart(M5Canvas& canvas, const float alpha)
		{
			const float brightness = outputs.activation_amount;

			const int center_x = 160;
			const int center_y = 120;
			const int radius = static_cast<int>(75 + 15 * brightness);

			constexpr float color_scale = 0.2f;
			const float scaled_brightness = (1.0f - color_scale) + (color_scale * brightness);
			const float red_float = 255.0f * alpha * scaled_brightness;
			const uint8_t red = static_cast<uint8_t>(std::clamp(red_float, 0.0f, 255.0f));

			const uint16_t color = canvas.color565(red, 0, 0);

			canvas.fillCircle(center_x, center_y, radius, color);
		}

		// ------------------------------------------------------------------------------------------------
		// Helper: Draw one “filled quad” between two concentric radii at two consecutive angles.
		// We break each quad into two triangles for M5Canvas’s fillTriangle(…) call.
		//    (xi0, yi0) = inner‐circle, angle = angleRad
		//    (xo0, yo0) = outer‐circle, angle = angleRad
		//    (xi1, yi1) = inner‐circle, angle = angleRadNext
		//    (xo1, yo1) = outer‐circle, angle = angleRadNext
		// ------------------------------------------------------------------------------------------------
		static void drawFilledQuad(M5Canvas& canvas, int xi0, int yi0, int xo0, int yo0, int xo1, int yo1, int xi1, int yi1, uint16_t color)
		{
			// First triangle: (xi0,yi0) → (xo0,yo0) → (xo1,yo1)
			canvas.fillTriangle(xi0, yi0, xo0, yo0, xo1, yo1, color);
			// Second triangle: (xo1,yo1) → (xi1,yi1) → (xi0,yi0)
			canvas.fillTriangle(xo1, yo1, xi1, yi1, xi0, yi0, color);
		}

		// ------------------------------------------------------------------------------------------------
		// draw_stats: Draw three curved “brackets” (( O )) around the heart. Each bracket‐pair is a 90° arc
		// on the left and a 90° arc on the right. We render the “outline” in solid WHITE, then overlay the
		// “filled” portion in the bar’s color up to the current fraction. Labels sit at the very bottom (or
		// top) of each bracket‐pair, centered horizontally.
		// ------------------------------------------------------------------------------------------------
		void draw_stats(M5Canvas& canvas, const HeartbeatInputs& in)
		{
			static constexpr int MAX_HEART_RADIUS = 90; // Maximum radius of the heart itself
			static constexpr int BASE_OFFSET = 20;		// How much farther out the innermost bracket sits (beyond MAX_HEART_RADIUS)
			static constexpr int BAR_THICKNESS = 10;	// Thickness of each bracket‐ring (same “height” as your original horizontal bars)
			static constexpr int BAR_SPACING = 6;		// Gap (in pixels) between nested bracket‐rings
			static constexpr int ANGLE_STEPS = 45;		// Number of segments used to approximate each 90° half‐bracket
			static constexpr int LABEL_ANGLE = 305;		// Angle at which to draw our labels

			struct StatBar
			{
				const char* label;
				float fraction;
				uint16_t color;
			};

			StatBar bars[] = {{in.bar1_label.c_str(), in.bar1_fraction, GREEN},
				{in.bar2_label.c_str(), in.bar2_fraction, YELLOW},
				{in.bar3_label.c_str(), in.bar3_fraction, BLUE},
				{in.bar4_label.c_str(), in.bar4_fraction, ORANGE}};

			const int N = sizeof(bars) / sizeof(bars[0]);
			if (N == 0)
			{
				return;
			}

			canvas.setTextSize(1);
			canvas.setTextColor(WHITE);
			canvas.setTextDatum(MC_DATUM);

			const int COLOR_UNFILLED = canvas.color565(50, 30, 30);
			const int LEFT_COUNT = (N + 1) / 2; // more on left if odd
			const int RIGHT_COUNT = N / 2;

			auto draw_arc = [&](const StatBar& sb, int index_on_side, bool left_side)
			{
				int inner_radius = MAX_HEART_RADIUS + BASE_OFFSET + index_on_side * (BAR_THICKNESS + BAR_SPACING);
				int outer_radius = inner_radius + BAR_THICKNESS;

				float frac_clamped = std::clamp(sb.fraction, 0.0f, 1.0f);
				int fill_steps = static_cast<int>(std::round(frac_clamped * ANGLE_STEPS));

				float start_deg = left_side ? 135.0f : 315.0f;

				for (int s = 0; s < ANGLE_STEPS; ++s)
				{
					float angleDeg0 = start_deg + (90.0f * s) / ANGLE_STEPS;
					float angleDeg1 = start_deg + (90.0f * (s + 1)) / ANGLE_STEPS;
					float a0 = angleDeg0 * (M_PI / 180.0f);
					float a1 = angleDeg1 * (M_PI / 180.0f);

					int xi0 = 160 + static_cast<int>(inner_radius * cosf(a0));
					int yi0 = 120 - static_cast<int>(inner_radius * sinf(a0));
					int xo0 = 160 + static_cast<int>(outer_radius * cosf(a0));
					int yo0 = 120 - static_cast<int>(outer_radius * sinf(a0));

					int xi1 = 160 + static_cast<int>(inner_radius * cosf(a1));
					int yi1 = 120 - static_cast<int>(inner_radius * sinf(a1));
					int xo1 = 160 + static_cast<int>(outer_radius * cosf(a1));
					int yo1 = 120 - static_cast<int>(outer_radius * sinf(a1));

					drawFilledQuad(canvas, xi0, yi0, xo0, yo0, xo1, yo1, xi1, yi1, COLOR_UNFILLED);

					bool should_fill = left_side ? (s >= ANGLE_STEPS - fill_steps) : (s < fill_steps);

					if (should_fill)
					{
						drawFilledQuad(canvas, xi0, yi0, xo0, yo0, xo1, yo1, xi1, yi1, sb.color);
					}
				}

				// Label at arc bottom (270°) or top (90°)
				float label_angle_deg = LABEL_ANGLE;
				float label_angle_rad = label_angle_deg * (M_PI / 180.0f);
				const float label_radius = 0.5f * (inner_radius + outer_radius);

				int label_x = 160 + static_cast<int>(label_radius * cosf(label_angle_rad)) * (left_side ? -1 : 1);
				int label_y = 120 - static_cast<int>(label_radius * sinf(label_angle_rad));

				canvas.drawString(sb.label, label_x, label_y);
			};

			// Draw left-side bars
			for (int i = 0; i < LEFT_COUNT; ++i)
			{
				draw_arc(bars[i], i, true);
			}

			// Draw right-side bars
			for (int i = 0; i < RIGHT_COUNT; ++i)
			{
				draw_arc(bars[LEFT_COUNT + i], i, false);
			}

			canvas.setTextDatum(TL_DATUM); // Reset
		}
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
	};

	ROBOTICK_REGISTER_WORKLOAD(HeartbeatDisplayWorkload, HeartbeatConfig, HeartbeatInputs, HeartbeatOutputs)

} // namespace robotick
