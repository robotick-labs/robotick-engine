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

		// Call with null or empty string to trigger fallback listing behavior
		bool setup(const int camera_index);

		// On success, fills data_ptr/size with JPEG frame data
		bool read_frame(uint8_t* dst_buffer, const size_t dst_capacity, size_t& out_size_used);

		// Print available camera IDs (friendly or index-based)
		void print_available_cameras();

	  private:
		Impl* impl = nullptr;
	};

} // namespace robotick
