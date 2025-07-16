#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/platform/Camera.h"
#include "robotick/api.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace robotick
{
	struct Camera::Impl
	{
		cv::VideoCapture video_capture;
		std::vector<uchar> jpeg_buffer;
		cv::Mat fallback_image;
	};

	static void init_fallback_image(cv::Mat& fallback_image)
	{
		fallback_image = cv::Mat(480, 640, CV_8UC3, cv::Scalar(255, 255, 255)); // White
		cv::putText(fallback_image, "Robotick|No Camera", cv::Point(20, 240), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 128), 2);
	}

	Camera::Camera()
	{
		impl = new Camera::Impl();

		init_fallback_image(impl->fallback_image);
	}

	Camera::~Camera()
	{
		// nothing to do yet
	}

	bool Camera::setup(const int camera_index)
	{
		if (camera_index < 0)
			return false;

		impl->video_capture.open(camera_index);
		return impl->video_capture.isOpened();
	}

	bool Camera::read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used)
	{
		cv::Mat frame;
		const bool camera_ready = impl->video_capture.isOpened() && impl->video_capture.read(frame);

		if (!camera_ready)
		{
			static bool warned = false;
			if (!warned)
			{
				ROBOTICK_WARNING("No camera available — using fallback test image.");
				warned = true;
			}
			frame = impl->fallback_image.clone(); // ensure mutability for cropping
		}

		// ✅ Always perform the crop+resize logic
		const float input_aspect = static_cast<float>(frame.cols) / frame.rows;
		const float target_aspect = 4.0f / 3.0f;

		int crop_x = 0, crop_y = 0, crop_w = frame.cols, crop_h = frame.rows;

		if (input_aspect > target_aspect)
		{
			crop_w = static_cast<int>(frame.rows * target_aspect);
			crop_x = (frame.cols - crop_w) / 2;
		}
		else if (input_aspect < target_aspect)
		{
			crop_h = static_cast<int>(frame.cols / target_aspect);
			crop_y = (frame.rows - crop_h) / 2;
		}

		cv::Rect crop_rect(crop_x, crop_y, crop_w, crop_h);
		cv::Mat cropped = frame(crop_rect);
		cv::resize(cropped, frame, cv::Size(640, 480));

		// ✅ Encode to JPEG
		impl->jpeg_buffer.clear();
		cv::imencode(".jpg", frame, impl->jpeg_buffer);

		if (impl->jpeg_buffer.size() <= dst_capacity)
		{
			memcpy(dst_buffer, impl->jpeg_buffer.data(), impl->jpeg_buffer.size());
			out_size_used = impl->jpeg_buffer.size();
			return true;
		}

		return false;
	}

	void Camera::print_available_cameras()
	{
		for (int camera_index = 0; camera_index < 10; ++camera_index)
		{
			cv::VideoCapture test(camera_index);
			if (test.isOpened())
			{
				ROBOTICK_INFO("Camera available: id='%i'", camera_index);
				test.release();
			}
		}
		ROBOTICK_INFO("Specify camera_index in config to select.");
	}
} // namespace robotick

#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
