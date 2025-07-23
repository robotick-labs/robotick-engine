#if defined(ROBOTICK_PLATFORM_ESP32)

#if defined(ENABLE_ESP32_CAMERA_CODE)

#include "robotick/api.h"
#include "robotick/platform/Camera.h"

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robotick
{
	struct Camera::Impl
	{
	};

	Camera::Camera()
	{
		impl = new Impl();
	}

	Camera::~Camera()
	{
		delete impl;
	}

	bool Camera::setup(const int camera_index)
	{
		(void)camera_index;

		camera_config_t config = {};
		config.pin_sccb_sda = 12;
		config.pin_sccb_scl = 11;
		config.pin_d7 = 47;
		config.pin_d6 = 48;
		config.pin_d5 = 16;
		config.pin_d4 = 15;
		config.pin_d3 = 42;
		config.pin_d2 = 41;
		config.pin_d1 = 40;
		config.pin_d0 = 39;
		config.pin_vsync = 46;
		config.pin_href = 38;
		config.pin_pclk = 45;

		config.pin_pwdn = -1;
		config.pin_reset = -1;
		config.pin_xclk = 14;

		config.xclk_freq_hz = 16000000;
		config.ledc_timer = LEDC_TIMER_0;
		config.ledc_channel = LEDC_CHANNEL_0;

		config.pixel_format = PIXFORMAT_RGB565;
		config.frame_size = FRAMESIZE_VGA;
		config.jpeg_quality = 10;
		config.fb_count = 1;
		config.fb_location = CAMERA_FB_IN_PSRAM;
		config.grab_mode = CAMERA_GRAB_LATEST;
		config.sccb_i2c_port = -1;

		esp_err_t err = esp_camera_init(&config);
		if (err != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("esp_camera_init failed: 0x%x", err);
			return false;
		}

		for (int i = 0; i < 5; ++i)
		{
			ROBOTICK_INFO("esp_camera_init - settling %i/5", i);
			camera_fb_t* fb = esp_camera_fb_get();
			if (fb)
				esp_camera_fb_return(fb);
			vTaskDelay(pdMS_TO_TICKS(100)); // allow settling
		}

		ROBOTICK_INFO("esp_camera_init success");
		return true;
	}

	static unsigned int jpeg_output_cb(void* arg, unsigned int index, const void* data, unsigned int len)
	{
		uint8_t* out = static_cast<uint8_t*>(arg);
		memcpy(out + index, data, len);
		return len;
	}

	bool Camera::read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used)
	{
		ROBOTICK_INFO("Camera::read_frame - acquiring frame");

		const int64_t start_us = esp_timer_get_time();

		camera_fb_t* fb = esp_camera_fb_get();

		const int64_t got_frame_us = esp_timer_get_time();

		if (!fb)
		{
			ROBOTICK_WARNING("Camera frame not ready");
			return false;
		}

		ROBOTICK_INFO("Captured %dx%d frame (%d bytes)", fb->width, fb->height, fb->len);

		ROBOTICK_INFO("Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
		ROBOTICK_INFO("Largest block: %d", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

		ROBOTICK_INFO("fb: width=%d, height=%d, len=%d (expected %d)", fb->width, fb->height, fb->len, fb->width * fb->height * 2);

		size_t jpeg_len = dst_capacity;

		const int64_t encode_start_us = esp_timer_get_time();

		bool ok = fmt2jpg_cb(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, 75, jpeg_output_cb, dst_buffer);

		const int64_t encode_end_us = esp_timer_get_time();

		esp_camera_fb_return(fb);

		if (!ok)
		{
			ROBOTICK_WARNING("fmt2jpg_cb() failed");
			return false;
		}

		// Find JPEG end marker (0xFFD9)
		for (jpeg_len = 2; jpeg_len < dst_capacity - 1; ++jpeg_len)
		{
			if (dst_buffer[jpeg_len] == 0xFF && dst_buffer[jpeg_len + 1] == 0xD9)
			{
				jpeg_len += 2;
				break;
			}
		}

		if (jpeg_len >= dst_capacity)
		{
			ROBOTICK_WARNING("JPEG frame overran buffer");
			return false;
		}

		out_size_used = jpeg_len;

		const int64_t end_us = esp_timer_get_time();

		ROBOTICK_INFO("TIMING (ms): total=%.2f, capture=%.2f, encode=%.2f",
			(end_us - start_us) / 1000.0,
			(got_frame_us - start_us) / 1000.0,
			(encode_end_us - encode_start_us) / 1000.0);

		ROBOTICK_INFO("JPEG frame ready (%d bytes)", (int)jpeg_len);
		return true;
	}

	void Camera::print_available_cameras()
	{
		ROBOTICK_INFO("ESP32 camera (RGB565) available at index 0");
	}

} // namespace robotick

#else // #if defined(ENABLE_ESP32_CAMERA_CODE)

#include "robotick/api.h"
#include "robotick/platform/Camera.h"

namespace robotick
{
	struct Camera::Impl
	{
	};

	Camera::Camera()
	{
		impl = new Impl();
	}

	Camera::~Camera()
	{
		delete impl;
	}

	bool Camera::setup(const int camera_index)
	{
		(void)camera_index;
		return true;
	}

	bool Camera::read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used)
	{
		(void)dst_buffer;
		(void)dst_capacity;
		out_size_used = 0;
		return false;
	}

	void Camera::print_available_cameras()
	{
		ROBOTICK_INFO("Camera stubs active (ESP32)");
	}

} // namespace robotick

#endif // #if defined(ENABLE_ESP32_CAMERA_CODE)
#endif // ROBOTICK_PLATFORM_ESP32
