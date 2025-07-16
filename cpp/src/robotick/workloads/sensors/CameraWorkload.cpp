// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/common/FixedVector.h"
#include "robotick/platform/Camera.h"

namespace robotick
{

	//------------------------------------------------------------------------------
	// Config / Inputs / Outputs
	//------------------------------------------------------------------------------

	struct CameraConfig
	{
		int camera_index = 0;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(CameraConfig)
	ROBOTICK_STRUCT_FIELD(CameraConfig, int, camera_index)
	ROBOTICK_REGISTER_STRUCT_END(CameraConfig)

	struct CameraInputs
	{
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(CameraInputs)
	ROBOTICK_REGISTER_STRUCT_END(CameraInputs)

	struct CameraOutputs
	{
		FixedVector128k jpeg_data;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(CameraOutputs)
	ROBOTICK_STRUCT_FIELD(CameraOutputs, FixedVector128k, jpeg_data)
	ROBOTICK_REGISTER_STRUCT_END(CameraOutputs)

	//------------------------------------------------------------------------------
	// State (not currently needed, but here for expansion)
	//------------------------------------------------------------------------------

	struct CameraState
	{
		Camera camera;
	};

	struct CameraWorkload
	{
		CameraConfig config;
		CameraInputs inputs;
		CameraOutputs outputs;
		State<CameraState> state;

		void load()
		{
			if (!state->camera.setup(config.camera_index))
			{
				state->camera.print_available_cameras();
				ROBOTICK_FATAL_EXIT("CameraWorkload failed to initialize camera index %i", config.camera_index);
			}
		}

		void tick(const TickInfo& tick_info)
		{
			(void)tick_info;

			size_t size_used = 0;
			if (state->camera.read_frame(outputs.jpeg_data.data(), outputs.jpeg_data.capacity(), size_used))
			{
				outputs.jpeg_data.set_size(size_used);
			}
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(CameraWorkload, CameraConfig, CameraInputs, CameraOutputs)

} // namespace robotick
