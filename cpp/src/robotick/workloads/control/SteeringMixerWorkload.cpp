// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#include <algorithm>

namespace robotick
{

	// === Field registrations ===

	struct SteeringMixerConfig
	{
		float max_speed_differential = 0.4f;
		float power_scale_both = 1.0f;
		float power_scale_left = 1.0f;
		float power_scale_right = 1.0f;
	};

	ROBOTICK_REGISTER_STRUCT_BEGIN(SteeringMixerConfig)
	ROBOTICK_STRUCT_FIELD(SteeringMixerConfig, float, max_speed_differential)
	ROBOTICK_STRUCT_FIELD(SteeringMixerConfig, float, power_scale_both)
	ROBOTICK_STRUCT_FIELD(SteeringMixerConfig, float, power_scale_left)
	ROBOTICK_STRUCT_FIELD(SteeringMixerConfig, float, power_scale_right)
	ROBOTICK_REGISTER_STRUCT_END(SteeringMixerConfig)

	struct SteeringMixerInputs
	{
		float speed = 0.0f;
		float turn_rate = 0.0f;
	};

	ROBOTICK_REGISTER_STRUCT_BEGIN(SteeringMixerInputs)
	ROBOTICK_STRUCT_FIELD(SteeringMixerInputs, float, speed)
	ROBOTICK_STRUCT_FIELD(SteeringMixerInputs, float, turn_rate)
	ROBOTICK_REGISTER_STRUCT_END(SteeringMixerInputs)

	struct SteeringMixerOutputs
	{
		float left_motor = 0.0f;
		float right_motor = 0.0f;
	};

	ROBOTICK_REGISTER_STRUCT_BEGIN(SteeringMixerOutputs)
	ROBOTICK_STRUCT_FIELD(SteeringMixerOutputs, float, left_motor)
	ROBOTICK_STRUCT_FIELD(SteeringMixerOutputs, float, right_motor)
	ROBOTICK_REGISTER_STRUCT_END(SteeringMixerOutputs)

	// === Workload ===

	struct SteeringMixerWorkload
	{
		SteeringMixerInputs inputs;
		SteeringMixerOutputs outputs;
		SteeringMixerConfig config;

		void tick(const TickInfo&)
		{
			const float speed = inputs.speed;
			const float turn = inputs.turn_rate;

			float left = speed + turn * config.max_speed_differential;
			float right = speed - turn * config.max_speed_differential;

			// Clamp to [-1, 1]
			left = std::max(-1.0f, std::min(1.0f, left));
			right = std::max(-1.0f, std::min(1.0f, right));

			left *= config.power_scale_both * config.power_scale_left;
			right *= config.power_scale_both * config.power_scale_right;

			outputs.left_motor = left;
			outputs.right_motor = right;
		}
	};

	// === Auto-registration ===

	ROBOTICK_REGISTER_WORKLOAD(SteeringMixerWorkload, SteeringMixerConfig, SteeringMixerInputs, SteeringMixerOutputs)

} // namespace robotick
