#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/platform/Camera.h"
#include "robotick/api.h"

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace robotick
{
	static const size_t MaxJpegSize = 64 * 1024;

	struct Camera::Impl
	{
		camera_fb_t* fb;
		uint8_t* jpeg_ptr;
		size_t jpeg_len;

		Impl()
			: fb(nullptr)
			, jpeg_ptr(nullptr)
			, jpeg_len(0)
		{
		}
	};

	Camera::Camera()
	{
		impl = new Impl();
	}

	Camera::~Camera()
	{
		if (impl->fb)
			esp_camera_fb_return(impl->fb);

		if (impl->jpeg_ptr)
			free(impl->jpeg_ptr);

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
		config.pin_xclk = -1;

		config.xclk_freq_hz = 20000000;
		config.ledc_timer = LEDC_TIMER_0;
		config.ledc_channel = LEDC_CHANNEL_0;

		config.pixel_format = PIXFORMAT_RGB565;
		config.frame_size = FRAMESIZE_QVGA;
		config.jpeg_quality = 10;
		config.fb_count = 1;
		config.fb_location = CAMERA_FB_IN_PSRAM;
		config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
		config.sccb_i2c_port = -1;

		esp_err_t err = esp_camera_init(&config);
		if (err != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("esp_camera_init failed: 0x%x", err);
			return false;
		}

		ROBOTICK_INFO("esp_camera_init success");
		return true;
	}

	bool Camera::read_frame(const uint8_t*& out_data_ptr, size_t& out_size)
	{
		if (impl->fb)
		{
			esp_camera_fb_return(impl->fb);
			impl->fb = nullptr;
		}
		if (impl->jpeg_ptr)
		{
			free(impl->jpeg_ptr);
			impl->jpeg_ptr = nullptr;
			impl->jpeg_len = 0;
		}

		impl->fb = esp_camera_fb_get();
		if (!impl->fb)
		{
			ROBOTICK_WARNING("Camera frame not ready");
			return false;
		}

		// Encode to JPEG directly from the framebuffer (RGB565 -> JPEG)
		uint8_t* out_jpg = nullptr;
		size_t out_jpg_len = 0;

		bool ok = frame2jpg(impl->fb, 80 /*quality*/, &out_jpg, &out_jpg_len);
		if (!ok || !out_jpg)
		{
			ROBOTICK_WARNING("frame2jpg() failed");
			return false;
		}

		impl->jpeg_ptr = out_jpg;
		impl->jpeg_len = out_jpg_len;

		out_data_ptr = impl->jpeg_ptr;
		out_size = impl->jpeg_len;
		return true;
	}

	void Camera::print_available_cameras()
	{
		ROBOTICK_INFO("ESP32 camera (RGB565) available at index 0");
	}

} // namespace robotick

#endif // ROBOTICK_PLATFORM_ESP32
