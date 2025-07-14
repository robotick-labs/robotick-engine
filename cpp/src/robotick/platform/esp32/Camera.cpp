#if defined(ROBOTICK_PLATFORM_ESP32)

#include "robotick/platform/Camera.h"
#include "esp_camera.h"

namespace robotick::Camera
{
	static camera_fb_t* last_frame = nullptr;

	Camera::Camera(){};
	Camera::~Camera(){};

	bool Camera::setup(const int camera_index)
	{
		(void)camera_index; // Only one camera supported on CoreS3 for now

		camera_config_t cfg = {};
		cfg.ledc_channel = LEDC_CHANNEL_0;
		cfg.ledc_timer = LEDC_TIMER_0;
		cfg.pin_d0 = ...; // your actual CoreS3 pin mapping
		cfg.xclk_freq_hz = 20000000;
		cfg.pixel_format = PIXFORMAT_JPEG;
		cfg.frame_size = FRAMESIZE_VGA; // ✅ Force 640x480
		cfg.jpeg_quality = 10;			// 0 = best, 63 = worst
		cfg.fb_count = 1;

		return esp_camera_init(&cfg) == ESP_OK;
	}

	bool Camera::read_frame(const uint8_t*& out_data_ptr, size_t& out_size)
	{
		if (last_frame)
			esp_camera_fb_return(last_frame);
		last_frame = esp_camera_fb_get();
		if (!last_frame)
			return false;

		out_data_ptr = last_frame->buf;
		out_size = last_frame->len;

		return true;
	}

	void Camera::print_available_cameras()
	{
		ROBOTICK_LOG_INFO("ESP32 platform: single camera is auto-selected.");
	}
} // namespace robotick::Camera

#endif // #if defined(ROBOTICK_PLATFORM_ESP32)
