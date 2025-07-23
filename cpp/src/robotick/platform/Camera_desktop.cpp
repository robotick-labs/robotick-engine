#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/api.h"
#include "robotick/platform/Camera.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <thread>
#include <vector>

namespace robotick
{

	/// @brief Continuously captures JPEG frames from a camera in a background thread.
	///
	/// Frames are read from OpenCV's VideoCapture, encoded to JPEG, and made available via get_latest_frame().
	/// This allows the main engine loop to query for the most recent frame without blocking.

	class AsyncCameraCapture
	{
	  public:
		/// @brief Constructs the capture object. Call setup() to begin streaming.
		AsyncCameraCapture() = default;

		/// @brief Destructor — automatically stops the capture thread.
		~AsyncCameraCapture() { stop(); }

		/// @brief Sets up the camera and starts the background capture thread.
		/// @param camera_index Index of the camera to open.
		/// @return true if the camera was opened successfully, false otherwise.
		bool setup(int camera_index = 0)
		{
			video_capture.open(camera_index, cv::CAP_V4L2);
			if (!video_capture.isOpened())
				return false;

			video_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
			video_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
			video_capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

			for (int i = 0; i < 5; ++i)
				video_capture.grab();

			running = true;
			thread = std::thread(
				[this]()
				{
					cv::Mat frame;
					while (running)
					{
						if (video_capture.read(frame))
						{
							std::vector<uchar> temp;
							cv::imencode(".jpg", frame, temp);
							std::lock_guard<std::mutex> lock(mutex);
							jpeg_data = std::move(temp);
							std::this_thread::sleep_for(std::chrono::milliseconds(1));
						}
						else
						{
							// wait half a second before retrying
							std::this_thread::sleep_for(std::chrono::milliseconds(500));
						}
					}
				});

			return true;
		}

		/// @brief Retrieves the most recently captured JPEG frame, if available.
		/// @param dst_buffer Destination buffer to copy JPEG bytes into.
		/// @param dst_capacity Capacity of the destination buffer in bytes.
		/// @param out_size_used Number of bytes written to the destination.
		/// @return true if a valid frame was returned, false otherwise.
		bool get_latest_frame(uint8_t* dst_buffer, size_t dst_capacity, size_t& out_size_used)
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (jpeg_data.empty() || jpeg_data.size() > dst_capacity)
				return false;

			memcpy(dst_buffer, jpeg_data.data(), jpeg_data.size());
			out_size_used = jpeg_data.size();
			return true;
		}

	  private:
		/// @brief Stops the capture thread if running.
		void stop()
		{
			if (running.exchange(false) && thread.joinable())
				thread.join();
		}

		std::atomic<bool> running = false;
		std::thread thread;
		cv::VideoCapture video_capture;
		std::vector<uchar> jpeg_data;
		std::mutex mutex;
	};

	struct Camera::Impl
	{
		AsyncCameraCapture async_capture;
	};

	/// @brief Captures frames from a camera using OpenCV and exposes a simple blocking read interface.
	///
	/// This class uses an internal AsyncCameraCapture instance to decouple frame acquisition from engine tick rate.
	Camera::Camera()
	{
		impl = new Camera::Impl();
	}

	Camera::~Camera()
	{
		delete impl;
	}

	bool Camera::setup(const int camera_index)
	{
		if (camera_index < 0)
			return false;

		if (!impl->async_capture.setup(camera_index))
			return false;

		return true;
	}

	bool Camera::read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used)
	{
		if (impl->async_capture.get_latest_frame(dst_buffer, dst_capacity, out_size_used))
			return true;

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

#endif // ROBOTICK_PLATFORM_DESKTOP