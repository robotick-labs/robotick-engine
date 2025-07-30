// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/platform/Renderer.h"

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
		float bar1_fraction = 0.9f;
		FixedString8 bar2_label = "Ti";
		float bar2_fraction = 0.0f;
		FixedString8 bar3_label = "Ex";
		float bar3_fraction = 0.6f;
		FixedString8 bar4_label = "Hu";
		float bar4_fraction = 0.2f;

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

	struct HeartbeatState
	{
		bool has_init_renderer = false;
		Renderer renderer;
	};

	struct HeartbeatDisplayWorkload
	{
		HeartbeatConfig config;
		HeartbeatInputs inputs;
		HeartbeatOutputs outputs;
		State<HeartbeatState> state;

		void tick(const TickInfo& tick_info)
		{
			auto& s = state.get();

			if (!s.has_init_renderer)
			{
				s.renderer.init();
				s.has_init_renderer = true;
			}

			const float time_now_sec = tick_info.time_now;
			const float bpm = config.rest_heart_rate * inputs.heart_rate_scale;
			const float beat_duration = 60.0f / bpm;
			const float beat_phase = fmodf(time_now_sec, beat_duration) / beat_duration;

			// update ui:
			update_heart(beat_phase);

			// draw ui:
			s.renderer.clear();
			draw_heart(s.renderer, outputs.activation_amount);
			draw_stats(s.renderer, inputs);

			// present ui:
			s.renderer.present();
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

			auto ramp = [](float f)
			{
				return 0.5f * (1.0f - cosf(f * M_PI));
			};

			outputs.activation_amount = min_activation_lo;

			if (beat_phase >= lub_start && beat_phase < lub_start + lub_up)
				outputs.activation_amount = min_activation_lo + (max_activation - min_activation_hi) * ramp((beat_phase - lub_start) / lub_up);
			else if (beat_phase < lub_start + lub_up + lub_down)
				outputs.activation_amount =
					min_activation_hi + (max_activation - min_activation_hi) * (1.0f - ramp((beat_phase - lub_start - lub_up) / lub_down));
			else if (beat_phase < dub_start + dub_up)
				outputs.activation_amount = min_activation_hi + (max_activation - min_activation_hi) * ramp((beat_phase - dub_start) / dub_up);
			else if (beat_phase < dub_start + dub_up + dub_down)
				outputs.activation_amount =
					min_activation_hi + (max_activation - min_activation_hi) * (1.0f - ramp((beat_phase - dub_start - dub_up) / dub_down));
			else
			{
				const float settle = (beat_phase - (dub_start + dub_up + dub_down)) / (1.0f - (dub_start + dub_up + dub_down));
				outputs.activation_amount = min_activation_lo + (min_activation_hi - min_activation_lo) * (1.0f - std::clamp(settle, 0.0f, 1.0f));
			}
		}

		void draw_heart(Renderer& r, float brightness)
		{
			const Vec2 center(160, 120);
			const float radius = 75.f + 15.f * brightness;
			constexpr float color_scale = 0.2f;
			const float scaled = (1.0f - color_scale) + (color_scale * brightness);

			Color color;
			color.r = static_cast<uint8_t>(255.0f * scaled);

			r.draw_circle_filled(center, radius, color);
		}

		void draw_filled_quad(Renderer& r, float xi0, float yi0, float xo0, float yo0, float xo1, float yo1, float xi1, float yi1, const Color& c)
		{
			r.draw_triangle_filled({xi0, yi0}, {xo0, yo0}, {xo1, yo1}, c);
			r.draw_triangle_filled({xo1, yo1}, {xi1, yi1}, {xi0, yi0}, c);
		}

		void draw_stats(Renderer& r, const HeartbeatInputs& in)
		{
			static constexpr int BASE_RADIUS = 90;
			static constexpr int BASE_OFFSET = 20;
			static constexpr int BAR_THICKNESS = 10;
			static constexpr int BAR_SPACING = 6;
			static constexpr int ANGLE_STEPS = 24;
			static constexpr int LABEL_ANGLE = 305;

			struct StatBar
			{
				const char* label;
				float frac;
				Color color;
			};

			StatBar bars[] = {
				{in.bar1_label.c_str(), in.bar1_fraction, Colors::Green},
				{in.bar2_label.c_str(), in.bar2_fraction, Colors::Yellow},
				{in.bar3_label.c_str(), in.bar3_fraction, Colors::Blue},
				{in.bar4_label.c_str(), in.bar4_fraction, Colors::Orange},
			};

			const int N = sizeof(bars) / sizeof(StatBar);
			if (N == 0)
				return;

			const Color dim = {50, 30, 30, 255};
			const int LEFT_COUNT = (N + 1) / 2;
			const int RIGHT_COUNT = N / 2;

			auto draw_arc = [&](const StatBar& b, int index, bool left)
			{
				const float cx = 160.f, cy = 120.f;
				const float base_deg = left ? 135.0f : 315.0f;
				const float deg_span = 90.0f;

				const int r0 = BASE_RADIUS + BASE_OFFSET + index * (BAR_THICKNESS + BAR_SPACING);
				const int r1 = r0 + BAR_THICKNESS;
				const int fill_steps = static_cast<int>(std::round(std::clamp(b.frac, 0.0f, 1.0f) * ANGLE_STEPS));

				float cos_table[ANGLE_STEPS + 1], sin_table[ANGLE_STEPS + 1];
				for (int i = 0; i <= ANGLE_STEPS; ++i)
				{
					float angle = (base_deg + (deg_span * i) / ANGLE_STEPS) * M_PI / 180.0f;
					cos_table[i] = cosf(angle);
					sin_table[i] = sinf(angle);
				}

				for (int i = 0; i < ANGLE_STEPS; ++i)
				{
					const float xi0 = cx + r0 * cos_table[i];
					const float yi0 = cy - r0 * sin_table[i];
					const float xo0 = cx + r1 * cos_table[i];
					const float yo0 = cy - r1 * sin_table[i];
					const float xi1 = cx + r0 * cos_table[i + 1];
					const float yi1 = cy - r0 * sin_table[i + 1];
					const float xo1 = cx + r1 * cos_table[i + 1];
					const float yo1 = cy - r1 * sin_table[i + 1];

					bool fill = left ? (i >= ANGLE_STEPS - fill_steps) : (i < fill_steps);

					if (!fill || fill_steps < ANGLE_STEPS)
						draw_filled_quad(r, xi0, yi0, xo0, yo0, xo1, yo1, xi1, yi1, dim);

					if (fill)
						draw_filled_quad(r, xi0, yi0, xo0, yo0, xo1, yo1, xi1, yi1, b.color);
				}

				// Label
				if (b.label && b.label[0] != '\0')
				{
					const float label_deg = LABEL_ANGLE * M_PI / 180.0f;
					const float mid_r = 0.5f * (r0 + r1);
					const float label_x = cx + (mid_r * cosf(label_deg)) * (left ? -1.0f : 1.0f);
					const float label_y = cy - (mid_r * sinf(label_deg));
					r.draw_text(b.label, {label_x, label_y}, 12, TextAlign::Center, Colors::White);
				}
			};

			for (int i = 0; i < LEFT_COUNT; ++i)
				draw_arc(bars[i], i, true);

			for (int i = 0; i < RIGHT_COUNT; ++i)
				draw_arc(bars[LEFT_COUNT + i], i, false);
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(HeartbeatDisplayWorkload, HeartbeatConfig, HeartbeatInputs, HeartbeatOutputs)
} // namespace robotick
