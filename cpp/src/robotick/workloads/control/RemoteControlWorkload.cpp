// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/platform/WebServer.h"

namespace robotick
{
	struct RemoteControlConfig
	{
		int port = 7080;
		FixedString128 web_root_folder = "engine-data/remote_control_interface_web";
	};
	ROBOTICK_BEGIN_FIELDS(RemoteControlConfig)
	ROBOTICK_FIELD(RemoteControlConfig, int, port)
	ROBOTICK_FIELD(RemoteControlConfig, FixedString128, web_root_folder)
	ROBOTICK_END_FIELDS()

	struct RemoteControlInputs
	{
		bool use_web_inputs = true;

		double left_x = 0;
		double left_y = 0;
		double right_x = 0;
		double right_y = 0;

		double dead_zone_left_x = 0.0;
		double dead_zone_left_y = 0.0;
		double dead_zone_right_x = 0.0;
		double dead_zone_right_y = 0.0;

		double scale_left_x = 1.0;
		double scale_left_y = 1.0;
		double scale_right_x = 1.0;
		double scale_right_y = 1.0;
	};
	ROBOTICK_BEGIN_FIELDS(RemoteControlInputs)
	ROBOTICK_FIELD(RemoteControlInputs, bool, use_web_inputs)
	ROBOTICK_FIELD(RemoteControlInputs, double, left_x)
	ROBOTICK_FIELD(RemoteControlInputs, double, left_y)
	ROBOTICK_FIELD(RemoteControlInputs, double, right_x)
	ROBOTICK_FIELD(RemoteControlInputs, double, right_y)
	ROBOTICK_FIELD(RemoteControlInputs, double, dead_zone_left_x)
	ROBOTICK_FIELD(RemoteControlInputs, double, dead_zone_left_y)
	ROBOTICK_FIELD(RemoteControlInputs, double, dead_zone_right_x)
	ROBOTICK_FIELD(RemoteControlInputs, double, dead_zone_right_y)
	ROBOTICK_FIELD(RemoteControlInputs, double, scale_left_x)
	ROBOTICK_FIELD(RemoteControlInputs, double, scale_left_y)
	ROBOTICK_FIELD(RemoteControlInputs, double, scale_right_x)
	ROBOTICK_FIELD(RemoteControlInputs, double, scale_right_y)
	ROBOTICK_END_FIELDS()

	struct RemoteControlOutputs
	{
		double left_x = 0;
		double left_y = 0;
		double right_x = 0;
		double right_y = 0;
	};
	ROBOTICK_BEGIN_FIELDS(RemoteControlOutputs)
	ROBOTICK_FIELD(RemoteControlOutputs, double, left_x)
	ROBOTICK_FIELD(RemoteControlOutputs, double, left_y)
	ROBOTICK_FIELD(RemoteControlOutputs, double, right_x)
	ROBOTICK_FIELD(RemoteControlOutputs, double, right_y)
	ROBOTICK_END_FIELDS()

	struct RemoteControlState
	{
		WebServer server;
		RemoteControlInputs web_inputs;
	};

	struct RemoteControlWorkload
	{
		RemoteControlConfig config;
		RemoteControlInputs inputs;
		RemoteControlOutputs outputs;

		State<RemoteControlState> state;

		void setup()
		{
			state->server.start(config.port, config.web_root_folder.c_str(),
				[&](const WebRequest& request, WebResponse& response)
				{
					if (request.method == "POST")
					{
						if (request.uri == "/api/joystick_input")
						{
							response.status_code = 200; // OK
														/*TODO - make client send us object-wise json e.g.
							
														{
															"use_web_inputs": true,
															"left": { "x": 0.0, "y": 0.0 },
															"right": { "x": 0.0, "y": 0.0 },
															"dead_zone_left": { "x": 0.0, "y": 0.0 },
															"dead_zone_right": { "x": 0.0, "y": 0.0 },
															"scale_left": { "x": 1.0, "y": 1.0 },
															"scale_right": { "x": 1.0, "y": 1.0 }
															}*/
						}
					}
				} // <-- this was missing
			);
		}

		void tick(const TickInfo&)
		{
			// if either 'inputs' wants web_inputs (e.g. human user) to have control then honour that:
			const bool use_web_inputs = inputs.use_web_inputs || state->web_inputs.use_web_inputs;

			const RemoteControlInputs& inputs_ref = use_web_inputs ? state->web_inputs : inputs;

			outputs.left_x = apply_dead_zone(inputs_ref.left_x, inputs_ref.dead_zone_left_x) * inputs_ref.scale_left_x;
			outputs.left_y = apply_dead_zone(inputs_ref.left_y, inputs_ref.dead_zone_left_y) * inputs_ref.scale_left_y;
			outputs.right_x = apply_dead_zone(inputs_ref.right_x, inputs_ref.dead_zone_right_x) * inputs_ref.scale_right_x;
			outputs.right_y = apply_dead_zone(inputs_ref.right_y, inputs_ref.dead_zone_right_y) * inputs_ref.scale_right_y;
		}

		void stop() { state->server.stop(); }

		static double apply_dead_zone(double value, double dead_zone)
		{
			if (std::abs(value) < dead_zone)
				return 0.0;
			else
			{
				const double sign = (value > 0.0) ? 1.0 : -1.0;
				return ((std::abs(value) - dead_zone) / (1.0 - dead_zone)) * sign;
			}
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(RemoteControlWorkload, RemoteControlConfig, RemoteControlInputs, RemoteControlOutputs)

} // namespace robotick
