#pragma once

#include <cstddef>
#include <cstdint>

namespace robotick
{
	class Camera
	{
	  public:
		// Constructor (default)
		Camera();

		// Destructor (default)
		~Camera();

		// Call with null or empty string to trigger fallback listing behavior
		bool setup(const int camera_index);

		// On success, fills data_ptr/size with JPEG frame data
		bool read_frame(const uint8_t*& out_data_ptr, size_t& out_size);

		// Print available camera IDs (friendly or index-based)
		void print_available_cameras();

	  private:
		struct Impl;
		Impl* impl = nullptr;
	};

} // namespace robotick
