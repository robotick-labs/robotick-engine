// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/platform/WebServer.h"

#if defined(ROBOTICK_PLATFORM_DESKTOP)
#include <nlohmann/json.hpp>
#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)

namespace robotick
{
	struct RemoteControlConfig
	{
		int port = 7080;
		FixedString128 web_root_folder = "engine-data/remote_control_interface_web";
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteControlConfig)
	ROBOTICK_STRUCT_FIELD(RemoteControlConfig, int, port)
	ROBOTICK_STRUCT_FIELD(RemoteControlConfig, FixedString128, web_root_folder)
	ROBOTICK_REGISTER_STRUCT_END(RemoteControlConfig)

	struct RemoteControlInputs
	{
		bool use_web_inputs = true;

		Vec2f left;
		Vec2f right;

		Vec2f scale_left{1.0f, 1.0f};
		Vec2f scale_right{1.0f, 1.0f};

		FixedVector128k jpeg_data;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteControlInputs)
	ROBOTICK_STRUCT_FIELD(RemoteControlInputs, bool, use_web_inputs)
	ROBOTICK_STRUCT_FIELD(RemoteControlInputs, Vec2f, left)
	ROBOTICK_STRUCT_FIELD(RemoteControlInputs, Vec2f, right)
	ROBOTICK_STRUCT_FIELD(RemoteControlInputs, Vec2f, scale_left)
	ROBOTICK_STRUCT_FIELD(RemoteControlInputs, Vec2f, scale_right)
	ROBOTICK_STRUCT_FIELD(RemoteControlInputs, FixedVector128k, jpeg_data)
	ROBOTICK_REGISTER_STRUCT_END(RemoteControlInputs)

	struct RemoteControlOutputs
	{
		Vec2f left;
		Vec2f right;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteControlOutputs)
	ROBOTICK_STRUCT_FIELD(RemoteControlOutputs, Vec2f, left)
	ROBOTICK_STRUCT_FIELD(RemoteControlOutputs, Vec2f, right)
	ROBOTICK_REGISTER_STRUCT_END(RemoteControlOutputs)

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
#if defined(ROBOTICK_PLATFORM_DESKTOP)
			state->server.start(config.port,
				config.web_root_folder.c_str(),
				[&](const WebRequest& request, WebResponse& response)
				{
					if (request.method == "POST" && request.uri == "/api/joystick_input")
					{
						const auto json_opt = nlohmann::json::parse(request.body, nullptr, /*allow exceptions*/ false);
						if (json_opt.is_discarded())
						{
							response.status_code = 400;
							response.body.set_from_string("Invalid JSON format.");
							return;
						}

						const nlohmann::json& json = json_opt;

						auto& w = state->web_inputs;

						if (json.contains("use_web_inputs") && json["use_web_inputs"].is_boolean())
							w.use_web_inputs = json["use_web_inputs"].get<bool>();

						auto try_set_vec2_from_json = [&](const std::string& name, Vec2f& out_vec2)
						{
							if (!json.contains(name))
								return;
							const auto& obj = json[name];
							if (obj.is_object())
							{
								if (obj.contains("x") && obj["x"].is_number())
									out_vec2.x = obj["x"].get<float>();
								if (obj.contains("y") && obj["y"].is_number())
									out_vec2.y = obj["y"].get<float>();
							}
						};

						try_set_vec2_from_json("left", w.left);
						try_set_vec2_from_json("right", w.right);

						response.status_code = 200;
					}
					else if (request.method == "GET" && request.uri == "/api/jpeg_data")
					{
						response.body.set(inputs.jpeg_data.data(), inputs.jpeg_data.size());
						response.content_type = "image/jpeg";
						response.status_code = 200;
					}
				});
#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
		}

		void tick(const TickInfo&)
		{
			// if either 'inputs' wants web_inputs (e.g. human user) to have control then honour that:
			const bool use_web_inputs = inputs.use_web_inputs || state->web_inputs.use_web_inputs;

			const RemoteControlInputs& inputs_ref = use_web_inputs ? state->web_inputs : inputs;

			outputs.left.x = inputs_ref.left.x * inputs_ref.scale_left.x;
			outputs.left.y = inputs_ref.left.y * inputs_ref.scale_left.y;
			outputs.right.x = inputs_ref.right.x * inputs_ref.scale_right.x;
			outputs.right.y = inputs_ref.right.y * inputs_ref.scale_right.y;
		}

		void stop()
		{
			state->server.stop();
		}

		static float apply_dead_zone(float value, float dead_zone)
		{
			if (std::abs(value) < dead_zone)
				return 0.0;
			else
			{
				const float sign = (value > 0.0) ? 1.0 : -1.0;
				return ((std::abs(value) - dead_zone) / (1.0 - dead_zone)) * sign;
			}
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(RemoteControlWorkload, RemoteControlConfig, RemoteControlInputs, RemoteControlOutputs)

} // namespace robotick
