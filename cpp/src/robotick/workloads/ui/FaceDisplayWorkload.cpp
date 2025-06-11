// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include <M5Unified.h>

namespace robotick {

struct FaceDisplayConfig {
	float blink_min_interval_sec = 1.5f;
	float blink_max_interval_sec = 4.0f;
};
ROBOTICK_BEGIN_FIELDS(FaceDisplayConfig)
ROBOTICK_FIELD(FaceDisplayConfig, float, blink_min_interval_sec)
ROBOTICK_FIELD(FaceDisplayConfig, float, blink_max_interval_sec)
ROBOTICK_END_FIELDS()

struct FaceDisplayInputs {};  // No inputs yet
ROBOTICK_BEGIN_FIELDS(FaceDisplayInputs)
ROBOTICK_END_FIELDS()

struct FaceDisplayOutputs {}; // No outputs yet
ROBOTICK_BEGIN_FIELDS(FaceDisplayOutputs)
ROBOTICK_END_FIELDS()

struct FaceDisplayWorkload {
	FaceDisplayConfig config;
	FaceDisplayInputs inputs;
	FaceDisplayOutputs outputs;

	float eye_blink_progress[2] = {0, 0};
	float next_blink_time[2] = {0, 0};

	void setup() {
		M5.begin();
		M5.Lcd.setRotation(1);
		schedule_blink_pair(0.0f);
	}

	void tick(const TickInfo& tick_info) {
		const float now_sec = static_cast<float>(tick_info.time_now_ns * 1e-9f);
		update_blinks(now_sec);

		M5Canvas canvas(&M5.Lcd);
		canvas.createSprite(320, 240);

		draw_face(canvas);
		canvas.pushSprite(0, 0);
	}

	void update_blinks(float now_sec) {
		if (now_sec >= next_blink_time[0] || now_sec >= next_blink_time[1]) {
			// Start a new blink for both eyes
			eye_blink_progress[0] = 1.0f;
			eye_blink_progress[1] = 1.0f;
			schedule_blink_pair(now_sec);
		} else {
			// Decay blink progress for each eye
			for (int i = 0; i < 2; ++i) {
				if (eye_blink_progress[i] > 0.0f) {
					eye_blink_progress[i] -= 0.15f;
					if (eye_blink_progress[i] < 0.0f)
						eye_blink_progress[i] = 0.0f;
				}
			}
		}
	}

	void schedule_blink_pair(float now_sec) {
		const float min_sec = config.blink_min_interval_sec;
		const float max_sec = config.blink_max_interval_sec;
		const float random_interval = min_sec + ((float)rand() / RAND_MAX) * (max_sec - min_sec);

		// Small per-eye offset (±100ms)
		const float max_eye_offset = 0.1f;
		const float offset0 = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * max_eye_offset;
		const float offset1 = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * max_eye_offset;

		next_blink_time[0] = now_sec + random_interval + offset0;
		next_blink_time[1] = now_sec + random_interval + offset1;
	}

	void draw_face(M5Canvas& canvas) {
		canvas.fillScreen(TFT_LIGHTGREY);

		int center_y = 120;
		int eye_w = 30;
		int eye_h = 55;
		int eye_spacing = 160;

		for (int i = 0; i < 2; ++i) {
			int cx = 160 + (i == 0 ? -eye_spacing / 2 : eye_spacing / 2);
			float scale_y = 1.0f - 0.8f * eye_blink_progress[i];
			draw_eye(canvas, cx, center_y, eye_w, static_cast<int>(eye_h * scale_y));
		}
	}

	void draw_eye(M5Canvas& canvas, int cx, int cy, int rx, int ry) {
		canvas.fillEllipse(cx, cy, rx, ry, TFT_BLACK);  // pupil
		canvas.fillEllipse(cx - rx / 4, cy + ry / 3, rx / 3, ry / 4, TFT_WHITE); // highlight
	}
};

ROBOTICK_DEFINE_WORKLOAD(FaceDisplayWorkload, FaceDisplayConfig, FaceDisplayInputs, FaceDisplayOutputs)

} // namespace robotick
