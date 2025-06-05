
// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/MqttFieldSync.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/platform/MqttClient.h"

#include <memory>
#include <string>

namespace robotick
{
	//----------------------------------------------------------------------
	// Config, Inputs, Outputs
	//----------------------------------------------------------------------

	struct MqttClientConfig
	{
		FixedString64 broker_url = "mqtt://localhost";
		int broker_mqtt_port = 1883;
		FixedString64 root_topic_namespace = "robotick";
	};
	ROBOTICK_BEGIN_FIELDS(MqttClientConfig)
	ROBOTICK_FIELD(MqttClientConfig, FixedString64, broker_url)
	ROBOTICK_FIELD(MqttClientConfig, int, broker_mqtt_port)
	ROBOTICK_FIELD(MqttClientConfig, FixedString64, root_topic_namespace)
	ROBOTICK_END_FIELDS()

	//----------------------------------------------------------------------
	// Internal State
	//----------------------------------------------------------------------

	class MqttClientWorkloadState
	{
	  public:
		std::unique_ptr<MqttClient> mqtt;
		std::unique_ptr<MqttFieldSync> field_sync;
		const Engine* engine = nullptr;
	};

	//----------------------------------------------------------------------
	// Workload
	//----------------------------------------------------------------------

	struct MqttClientWorkload
	{
		MqttClientConfig config;

		State<MqttClientWorkloadState> state;

		void set_engine(const Engine& engine_in) { state->engine = &engine_in; }

		void load()
		{
			ROBOTICK_ASSERT_MSG(state->engine != nullptr, "Engine must be set before load()");

			// 1. Create and connect MQTT client

			const std::string broker = [&]()
			{
				std::string url = config.broker_url.c_str();
				if (url.back() == '/')
					url.pop_back(); // Remove trailing slash
				return url + ":" + std::to_string(config.broker_mqtt_port);
			}();

			const std::string client_id = "robotick::MqttClientWorkload";
			auto mqtt_client = std::make_unique<MqttClient>(broker, client_id);
			mqtt_client->connect();

			// 2. Create MqttFieldSync
			const std::string root_ns = config.root_topic_namespace.to_string();
			auto field_sync = std::make_unique<MqttFieldSync>(*const_cast<Engine*>(state->engine), root_ns, *mqtt_client);

			state->mqtt = std::move(mqtt_client);
			state->field_sync = std::move(field_sync);
		}

		void start(double)
		{
			// Subscribe and initial sync
			state->field_sync->subscribe_and_sync_startup();
		}

		void tick(const TickInfo&)
		{
			state->field_sync->apply_control_updates();
			state->field_sync->publish_state_fields();
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(MqttClientWorkload, MqttClientConfig)

} // namespace robotick