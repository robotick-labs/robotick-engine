// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

#include <algorithm>

namespace robotick
{

	// === Field registrations ===

	struct SteeringMixerTransformerConfig
	{
		double max_speed_differential = 0.4;
		double power_scale_both = 1.0;
		double power_scale_left = 1.0;
		double power_scale_right = 1.0;
	};

	ROBOTICK_BEGIN_FIELDS(SteeringMixerTransformerConfig)
	ROBOTICK_FIELD(SteeringMixerTransformerConfig, double, max_speed_differential)
	ROBOTICK_FIELD(SteeringMixerTransformerConfig, double, power_scale_both)
	ROBOTICK_FIELD(SteeringMixerTransformerConfig, double, power_scale_left)
	ROBOTICK_FIELD(SteeringMixerTransformerConfig, double, power_scale_right)
	ROBOTICK_END_FIELDS()

	struct SteeringMixerTransformerInputs
	{
		double speed = 0.0;
		double turn_rate = 0.0;
	};

	ROBOTICK_BEGIN_FIELDS(SteeringMixerTransformerInputs)
	ROBOTICK_FIELD(SteeringMixerTransformerInputs, double, speed)
	ROBOTICK_FIELD(SteeringMixerTransformerInputs, double, turn_rate)
	ROBOTICK_END_FIELDS()

	struct SteeringMixerTransformerOutputs
	{
		double left_motor = 0.0;
		double right_motor = 0.0;
	};

	ROBOTICK_BEGIN_FIELDS(SteeringMixerTransformerOutputs)
	ROBOTICK_FIELD(SteeringMixerTransformerOutputs, double, left_motor)
	ROBOTICK_FIELD(SteeringMixerTransformerOutputs, double, right_motor)
	ROBOTICK_END_FIELDS()

	// === Workload ===

	struct SteeringMixerWorkload
	{
		SteeringMixerTransformerInputs inputs;
		SteeringMixerTransformerOutputs outputs;
		SteeringMixerTransformerConfig config;

		void tick(const TickInfo&)
		{
			const double speed = inputs.speed;
			const double turn = inputs.turn_rate;

			double left = speed + turn * config.max_speed_differential;
			double right = speed - turn * config.max_speed_differential;

			// Clamp to [-1, 1]
			left = std::max(-1.0, std::min(1.0, left));
			right = std::max(-1.0, std::min(1.0, right));

			left *= config.power_scale_both * config.power_scale_left;
			right *= config.power_scale_both * config.power_scale_right;

			outputs.left_motor = left;
			outputs.right_motor = right;

			// ROBOTICK_INFO("\033[2J\033[H"); // Clear screen + move cursor to top-left
			//
			// ROBOTICK_INFO("SteeringMixerWorkload::tick() inputs: speed=%.3f, turn=%.3f outputs: left_motor=%.3f, right_motor=%.3f", speed, turn,
			// 	outputs.left_motor, outputs.right_motor);
		}
	};

	// === Auto-registration ===

	ROBOTICK_DEFINE_WORKLOAD(SteeringMixerWorkload, SteeringMixerTransformerConfig, SteeringMixerTransformerInputs, SteeringMixerTransformerOutputs)

} // namespace robotick
