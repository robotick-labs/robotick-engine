// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/platform/Renderer.h"

namespace robotick
{
	struct FaceDisplayConfig
	{
		float blink_min_interval_sec = 1.5f;
		float blink_max_interval_sec = 4.0f;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(FaceDisplayConfig)
	ROBOTICK_STRUCT_FIELD(FaceDisplayConfig, float, blink_min_interval_sec)
	ROBOTICK_STRUCT_FIELD(FaceDisplayConfig, float, blink_max_interval_sec)
	ROBOTICK_REGISTER_STRUCT_END(FaceDisplayConfig)

	struct FaceDisplayInputs
	{
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(FaceDisplayInputs)
	ROBOTICK_REGISTER_STRUCT_END(FaceDisplayInputs)

	struct FaceDisplayOutputs
	{
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(FaceDisplayOutputs)
	ROBOTICK_REGISTER_STRUCT_END(FaceDisplayOutputs)

	struct FaceDisplayState
	{
		float eye_blink_progress[2] = {0, 0};
		float next_blink_time[2] = {0, 0};

		bool has_init_renderer = false;
		Renderer renderer;
	};

	struct FaceDisplayWorkload
	{
		FaceDisplayConfig config;
		FaceDisplayInputs inputs;
		FaceDisplayOutputs outputs;
		State<FaceDisplayState> state;

		void setup() { schedule_blink_pair(0.0f); }

		void tick(const TickInfo& tick_info)
		{
			auto& s = state.get();

			// init the renderer if not already done so:
			if (!s.has_init_renderer)
			{
				s.renderer.init();
				s.has_init_renderer = true;
			}

			// update our animations
			const float time_now_sec = tick_info.time_now;
			update_blinks(time_now_sec);

			// draw & present our face
			s.renderer.clear(Colors::White);
			draw_face(s.renderer);
			s.renderer.present();
		}

		void update_blinks(const float time_now_sec)
		{
			auto& s = state.get();
			auto& blink = s.eye_blink_progress;
			auto& next_time = s.next_blink_time;

			if (time_now_sec >= next_time[0] || time_now_sec >= next_time[1])
			{
				blink[0] = 1.0f;
				blink[1] = 1.0f;
				schedule_blink_pair(time_now_sec);
			}
			else
			{
				for (int i = 0; i < 2; ++i)
				{
					if (blink[i] > 0.0f)
					{
						blink[i] -= 0.15f;
						if (blink[i] < 0.0f)
							blink[i] = 0.0f;
					}
				}
			}
		}

		void schedule_blink_pair(const float time_now_sec)
		{
			auto& next_time = state->next_blink_time;
			const float min_sec = config.blink_min_interval_sec;
			const float max_sec = config.blink_max_interval_sec;
			const float random_interval = min_sec + ((float)rand() / RAND_MAX) * (max_sec - min_sec);
			const float max_eye_offset = 0.1f;

			next_time[0] = time_now_sec + random_interval + ((((float)rand() / RAND_MAX) * 2.0f - 1.0f) * max_eye_offset);
			next_time[1] = time_now_sec + random_interval + ((((float)rand() / RAND_MAX) * 2.0f - 1.0f) * max_eye_offset);
		}

		void draw_face(Renderer& r)
		{
			auto& blink = state->eye_blink_progress;
			const int center_y = 120;
			const int eye_w = 30;
			const int eye_h = 55;
			const int eye_spacing = 160;

			for (int i = 0; i < 2; ++i)
			{
				const int cx = 160 + (i == 0 ? -eye_spacing / 2 : eye_spacing / 2);
				const float scale_y = 1.0f - 0.8f * blink[i];
				draw_eye(r, cx, center_y, eye_w, static_cast<int>(eye_h * scale_y));
			}
		}

		void draw_eye(Renderer& r, const int cx, const int cy, const int rx, const int ry)
		{
			r.draw_ellipse_filled(Vec2(cx, cy), rx, ry, {0, 0, 0, 255});
			r.draw_ellipse_filled(Vec2(cx + rx / 4, cy - ry / 3), rx / 3, ry / 4, {255, 255, 255, 255});
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(FaceDisplayWorkload, FaceDisplayConfig, FaceDisplayInputs, FaceDisplayOutputs)
} // namespace robotick
