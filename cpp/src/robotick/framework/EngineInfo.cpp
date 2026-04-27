// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/EngineInfo.h"

#include "robotick/framework/registry/TypeMacros.h"

namespace robotick
{
	ROBOTICK_REGISTER_STRUCT_BEGIN(EngineClockInfo)
	ROBOTICK_STRUCT_FIELD(EngineClockInfo, float, time_now)
	ROBOTICK_STRUCT_FIELD(EngineClockInfo, uint64_t, time_now_ns)
	ROBOTICK_STRUCT_FIELD(EngineClockInfo, uint64_t, tick_count)
	ROBOTICK_STRUCT_FIELD(EngineClockInfo, float, tick_rate_hz)
	ROBOTICK_STRUCT_FIELD(EngineClockInfo, float, dt_seconds_last)
	ROBOTICK_REGISTER_STRUCT_END(EngineClockInfo)

	ROBOTICK_REGISTER_STRUCT_BEGIN(EngineInfo)
	ROBOTICK_STRUCT_FIELD(EngineInfo, EngineClockInfo, clock)
	ROBOTICK_REGISTER_STRUCT_END(EngineInfo)

	extern "C" void robotick_force_register_engine_info_types()
	{
		// This function exists solely to force this TU to be retained.
	}

} // namespace robotick
