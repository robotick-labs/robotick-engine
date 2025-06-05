// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"
#include "robotick/platform/MqttClient.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace robotick
{
	//--------------------------------------------------------------------------
	// Config, Inputs, Outputs
	//--------------------------------------------------------------------------

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

	struct MqttClientInputs
	{
	};
	ROBOTICK_BEGIN_FIELDS(MqttClientInputs)
	ROBOTICK_END_FIELDS()

	struct MqttClientOutputs
	{
	};
	ROBOTICK_BEGIN_FIELDS(MqttClientOutputs)
	ROBOTICK_END_FIELDS()

	//--------------------------------------------------------------------------
	// Internal State
	//--------------------------------------------------------------------------

	class MqttClientWorkloadState
	{
	  public:
		std::unique_ptr<MqttClient> mqtt;
		std::unordered_map<std::string, nlohmann::json> last_published;
		std::unordered_map<std::string, nlohmann::json> updated_topics;

		const Engine* engine = nullptr;
		WorkloadsBuffer mirror_buffer;
	};

	//--------------------------------------------------------------------------
	// Workload
	//--------------------------------------------------------------------------

	struct MqttClientWorkload
	{
		MqttClientConfig config;
		MqttClientInputs inputs;
		MqttClientOutputs outputs;

		State<MqttClientWorkloadState> state;

		MqttClientWorkload() {}

		MqttClientWorkload(const MqttClientWorkload&) = delete;
		MqttClientWorkload& operator=(const MqttClientWorkload&) = delete;

		void set_engine(const Engine& engine_in)
		{
			state->engine = &engine_in;
			state->mirror_buffer.create_mirror_from(engine_in.get_workloads_buffer());
		}

		void load()
		{
			ROBOTICK_ASSERT_MSG(state->engine != nullptr, "Engine should have been set by now");
			// ROBOTICK_ASSERT(state->mirror_buffer.  != nullptr, "Engine should have been set by now");

			std::string broker = std::string(config.broker_url.c_str()) + ":" + std::to_string(config.broker_mqtt_port);
			std::string client_id = "robotick::MqttClientWorkload";

			state->mqtt = std::make_unique<MqttClient>(broker, client_id);

			state->mqtt->set_callback(
				[this](const std::string& topic, const std::string& payload)
				{
					if (topic.find("/outputs/") != std::string::npos)
					{
						return; // Skip outputs - they are intended to be read-only
					}

					nlohmann::json incoming;
					try
					{
						incoming = nlohmann::json::parse(payload);
					}
					catch (...)
					{
						ROBOTICK_WARNING("MqttClient - Ignoring malformed JSON from topic: %s", topic.c_str());
						return;
					}

					if (state->last_published.find(topic) != state->last_published.end() && state->last_published[topic] == incoming)
					{
						return; // we're just having our initial value reflected back at us - ignore it
					}

					state->updated_topics[topic] = incoming;
				});

			state->mqtt->connect();
		}

		void start(double)
		{
			state->mqtt->subscribe(std::string(config.root_topic_namespace.c_str()) + "/#");

			update_mirror_buffer();	   // mirror workloads buffer to reduce aliasing as we work with it later
			sync_all_fields_to_mqtt(); // send initial full state

			state->updated_topics.clear();
			// ^- ignore any topic-updates received before this point - mqtt should reflect our initial state, not the other way around.
		}

		void tick(const TickInfo&)
		{
			update_mirror_buffer(); // mirror workloads buffer to reduce aliasing as we work with it later

			sync_updated_topics_from_mqtt();
			detect_and_publish_changed_fields();
		}

		inline void update_mirror_buffer() { state->mirror_buffer.update_mirror_from(state->engine->get_workloads_buffer()); }

		void sync_all_fields_to_mqtt()
		{
			WorkloadFieldsIterator::for_each_workload_field(*state->engine, &state->mirror_buffer,
				[&](const WorkloadFieldView& view)
				{
					std::string topic = config.root_topic_namespace.to_string();

					if (!view.workload_info || !view.field_ptr)
						return;

					topic += "/" + view.workload_info->unique_name;

					if (!view.struct_info)
						return;

					topic += "/" + view.struct_info->local_name;

					if (!view.field_info)
						return;

					topic += "/" + view.field_info->name;

					TypeId field_type(view.field_info->type);

					if (view.subfield_info)
					{
						topic += "/" + view.subfield_info->name.to_string();
						field_type = view.subfield_info->type;
					}

					publish_if_changed(topic, view.field_ptr, field_type);
				});
		}

		void sync_updated_topics_from_mqtt()
		{
			for (const auto& updated_topic : state->updated_topics)
			{
				(void)updated_topic;
				// TODO: Fetch payload for this topic, then update blackboard fields as needed
				// You will parse and dispatch later
			}
			state->updated_topics.clear();
		}

		void detect_and_publish_changed_fields()
		{
			sync_all_fields_to_mqtt(); // fine to use this for both initial sync and on tick - it only updates MQTT for modified fields anyway
		}

		void publish_if_changed(const std::string& topic, void* field_ptr, const TypeId& field_type)
		{
			nlohmann::json payload;

			if (field_type == GET_TYPE_ID(int))
				payload = *reinterpret_cast<int*>(field_ptr);
			else if (field_type == GET_TYPE_ID(double))
				payload = *reinterpret_cast<double*>(field_ptr);
			else if (field_type == GET_TYPE_ID(FixedString64))
				payload = reinterpret_cast<FixedString64*>(field_ptr)->c_str();
			else if (field_type == GET_TYPE_ID(FixedString128))
				payload = reinterpret_cast<FixedString128*>(field_ptr)->c_str();

			if (state->last_published[topic] != payload)
			{
				state->mqtt->publish(topic, payload.dump(), true);
				state->last_published[topic] = payload;
			}
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(MqttClientWorkload, MqttClientConfig, MqttClientInputs, MqttClientOutputs)

} // namespace robotick
