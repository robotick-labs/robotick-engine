#pragma once

#include <cstddef>
#include <cstdint>

namespace robotick
{
	class Camera
	{
	  public:
		struct Impl;

		// Constructor (default)
		Camera();

		// Destructor (default)
		~Camera();

		// Non-copyable. Moving allowed.
		Camera(const Camera&) = delete;
		Camera& operator=(const Camera&) = delete;
		Camera(Camera&&) noexcept;
		Camera& operator=(Camera&&) noexcept;

		// Call with zero to obtain default camera (if present)
		bool setup(const int camera_index);

		// On success, fills data_ptr/size with JPEG frame data
		bool read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used);

		// Print available camera IDs (friendly or index-based)
		void print_available_cameras();

	  private:
		Impl* impl = nullptr;
	};

} // namespace robotick
