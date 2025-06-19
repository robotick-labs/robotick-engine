// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#if defined(ROBOTICK_PLATFORM_ESP32)
#include <M5Unified.h>
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)

namespace robotick
{

	struct FaceDisplayConfig
	{
		float blink_min_interval_sec = 1.5f;
		float blink_max_interval_sec = 4.0f;
	};
	ROBOTICK_BEGIN_FIELDS(FaceDisplayConfig)
	ROBOTICK_FIELD(FaceDisplayConfig, float, blink_min_interval_sec)
	ROBOTICK_FIELD(FaceDisplayConfig, float, blink_max_interval_sec)
	ROBOTICK_END_FIELDS()

	struct FaceDisplayInputs
	{
	};
	ROBOTICK_BEGIN_FIELDS(FaceDisplayInputs)
	ROBOTICK_END_FIELDS()

	struct FaceDisplayOutputs
	{
	};
	ROBOTICK_BEGIN_FIELDS(FaceDisplayOutputs)
	ROBOTICK_END_FIELDS()

	struct FaceDisplayState
	{
		float eye_blink_progress[2] = {0, 0};
		float next_blink_time[2] = {0, 0};

#if defined(ROBOTICK_PLATFORM_ESP32)
		M5Canvas* canvas = nullptr;
#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
	};

	struct FaceDisplayWorkload
	{
		FaceDisplayConfig config;
		FaceDisplayInputs inputs;
		FaceDisplayOutputs outputs;

		State<FaceDisplayState> state;

#if defined(ROBOTICK_PLATFORM_ESP32)

		~FaceDisplayWorkload()
		{
			auto& s = state.get();
			if (s.canvas)
			{
				delete s.canvas;
			}
		}

		void setup()
		{
			auto& s = state.get();
			M5.begin();
			M5.Lcd.setRotation(3);

			s.canvas = new M5Canvas(&M5.Lcd);
			s.canvas->createSprite(320, 240);

			schedule_blink_pair(0.0f);
		}

		void tick(const TickInfo& tick_info)
		{
			auto& s = state.get();
			auto& canvas = *s.canvas;

			const float now_sec = tick_info.time_now_ns * 1e-9f;
			update_blinks(now_sec);

			draw_face(canvas);
			canvas.pushSprite(0, 0);
		}

		void update_blinks(float now_sec)
		{
			auto& s = state.get();
			auto& blink = s.eye_blink_progress;
			auto& next_time = s.next_blink_time;

			if (now_sec >= next_time[0] || now_sec >= next_time[1])
			{
				blink[0] = 1.0f;
				blink[1] = 1.0f;
				schedule_blink_pair(now_sec);
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

		void schedule_blink_pair(float now_sec)
		{
			auto& next_time = state->next_blink_time;

			const float min_sec = config.blink_min_interval_sec;
			const float max_sec = config.blink_max_interval_sec;
			const float random_interval = min_sec + ((float)rand() / RAND_MAX) * (max_sec - min_sec);

			const float max_eye_offset = 0.1f;
			const float offset0 = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * max_eye_offset;
			const float offset1 = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * max_eye_offset;

			next_time[0] = now_sec + random_interval + offset0;
			next_time[1] = now_sec + random_interval + offset1;
		}

		void draw_face(M5Canvas& canvas)
		{
			auto& blink = state->eye_blink_progress;

			canvas.fillScreen(TFT_WHITE);

			const int center_y = 120;
			const int eye_w = 30;
			const int eye_h = 55;
			const int eye_spacing = 160;

			for (int i = 0; i < 2; ++i)
			{
				const int cx = 160 + (i == 0 ? -eye_spacing / 2 : eye_spacing / 2);
				const float scale_y = 1.0f - 0.8f * blink[i];
				draw_eye(canvas, cx, center_y, eye_w, static_cast<int>(eye_h * scale_y));
			}
		}

		void draw_eye(M5Canvas& canvas, int cx, int cy, int rx, int ry)
		{
			canvas.fillEllipse(cx, cy, rx, ry, TFT_BLACK);
			canvas.fillEllipse(cx + rx / 4, cy - ry / 3, rx / 3, ry / 4, TFT_WHITE);
		}

#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
	};

	ROBOTICK_DEFINE_WORKLOAD(FaceDisplayWorkload, FaceDisplayConfig, FaceDisplayInputs, FaceDisplayOutputs)

} // namespace robotick
