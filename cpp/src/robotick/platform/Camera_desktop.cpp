#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/platform/Camera.h"

#include <cstring>
#include <opencv2/opencv.hpp>
#include <vector>

namespace robotick
{
	class Camera::Impl
	{
	  public:
		cv::VideoCapture video_capture;
	};

	Camera::Camera()
	{
		impl = new Camera::Impl();
	}

	Camera::~Camera()
	{
		if (impl->video_capture.isOpened())
			impl->video_capture.release();
		delete impl;
	}

	bool Camera::setup(const int camera_index)
	{
		if (camera_index < 0)
			return false;

		if (!impl->video_capture.open(camera_index, cv::CAP_V4L2))
			return false;

		impl->video_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
		impl->video_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
		impl->video_capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

		return true;
	}

	bool Camera::read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used)
	{
		if (!impl->video_capture.isOpened())
			return false;

		if (!impl->video_capture.grab())
			return false;

		cv::Mat frame;
		if (!impl->video_capture.retrieve(frame))
			return false;

		std::vector<uchar> jpeg_data;
		if (!cv::imencode(".jpg", frame, jpeg_data))
			return false;

		if (jpeg_data.size() > dst_capacity)
			return false;

		std::memcpy(dst_buffer, jpeg_data.data(), jpeg_data.size());
		out_size_used = jpeg_data.size();
		return true;
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

#endif // ROBOTICK_PLATFORM_DESKTOP
