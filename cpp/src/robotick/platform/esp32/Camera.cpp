#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/platform/Camera.h"
#include "esp_camera.h"
#include "robotick/api.h"

namespace robotick
{
	struct Camera::Impl
	{
		const uint8_t* jpeg_ptr = nullptr;
		size_t jpeg_len = 0;
		camera_fb_t* fb = nullptr;
	};

	Camera::Camera()
	{
		impl = new Impl();
	}

	Camera::~Camera()
	{
		if (impl->fb)
			esp_camera_fb_return(impl->fb);
		delete impl;
	}

	bool Camera::setup(const int camera_index)
	{
		(void)camera_index;

		static camera_config_t config = {
			.pin_pwdn = -1,
			.pin_reset = -1,
			.pin_xclk = -1,
			.pin_sscb_sda = 12,
			.pin_sscb_scl = 11,
			.pin_d7 = 47,
			.pin_d6 = 48,
			.pin_d5 = 16,
			.pin_d4 = 15,
			.pin_d3 = 42,
			.pin_d2 = 41,
			.pin_d1 = 40,
			.pin_d0 = 39,

			.pin_vsync = 46,
			.pin_href = 38,
			.pin_pclk = 45,

			.xclk_freq_hz = 20000000,
			.ledc_timer = LEDC_TIMER_0,
			.ledc_channel = LEDC_CHANNEL_0,

			.pixel_format = PIXFORMAT_JPEG,
			.frame_size = FRAMESIZE_VGA, // 640x480
			.jpeg_quality = 10,
			.fb_count = 2,
			.fb_location = CAMERA_FB_IN_PSRAM,
			.grab_mode = CAMERA_GRAB_WHEN_EMPTY,
			.sccb_i2c_port = -1,
		};

		esp_err_t err = esp_camera_init(&config);
		if (err != ESP_OK)
		{
			ROBOTICK_FATAL_EXIT("esp_camera_init failed: 0x%x", err);
			return false;
		}

		sensor_t* s = esp_camera_sensor_get();
		if (s)
		{
			s->set_framesize(s, FRAMESIZE_VGA);
			s->set_quality(s, 10);
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

		impl->fb = esp_camera_fb_get();
		if (!impl->fb)
		{
			ROBOTICK_WARNING("Camera frame not ready");
			return false;
		}

		impl->jpeg_ptr = impl->fb->buf;
		impl->jpeg_len = impl->fb->len;

		out_data_ptr = impl->jpeg_ptr;
		out_size = impl->jpeg_len;
		return true;
	}

	void Camera::print_available_cameras()
	{
		ROBOTICK_INFO("ESP32 camera connected at fixed CoreS3 pinout (index 0)");
	}
} // namespace robotick

#endif // ROBOTICK_PLATFORM_ESP32
