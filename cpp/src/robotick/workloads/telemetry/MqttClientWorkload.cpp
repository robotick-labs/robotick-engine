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
			std::string broker = std::string(config.broker_url.c_str()) + ":" + std::to_string(config.broker_mqtt_port);
			std::string client_id = "robotick::MqttClientWorkload";

			state->mqtt = std::make_unique<MqttClient>(broker, client_id);

			state->mqtt->set_callback(
				[this](const std::string& topic, const std::string& payload)
				{
					if (topic.find(config.root_topic_namespace.to_string() + "/control/") != 0)
					{
						ROBOTICK_FATAL_EXIT("MqttClient - we should only be subscribed to topics in the '<root>/control/#' group");
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
						return;

					state->updated_topics[topic] = incoming;
				});

			state->mqtt->connect();
		}

		void start(double)
		{
			std::string root = config.root_topic_namespace.to_string();
			state->mqtt->subscribe(root + "/control/#");

			update_mirror_buffer();

			const bool should_publish_control = true; // also publish writeable fields to "control" topic-group once on startup
			sync_all_fields_to_mqtt(should_publish_control);

			state->updated_topics.clear();
		}

		void tick(const TickInfo&)
		{
			update_mirror_buffer(); // TODO - get ordering right vs below, which currently goes into main buffer - though perhaps it shouldn't?
			sync_updated_topics_from_mqtt();

			const bool should_publish_control = false;
			sync_all_fields_to_mqtt(should_publish_control);
		}

		inline void update_mirror_buffer() { state->mirror_buffer.update_mirror_from(state->engine->get_workloads_buffer()); }

		void sync_all_fields_to_mqtt(const bool should_publish_control)
		{
			WorkloadFieldsIterator::for_each_workload_field(*state->engine, &state->mirror_buffer,
				[&](const WorkloadFieldView& view)
				{
					if (!view.workload_info || !view.field_ptr || !view.struct_info || !view.field_info)
						return;

					std::string root = config.root_topic_namespace.to_string();
					std::string field_path = view.workload_info->unique_name + "/" + view.struct_info->local_name + "/" + view.field_info->name;

					TypeId field_type = view.field_info->type;
					if (view.subfield_info)
					{
						field_path += "/" + view.subfield_info->name.to_string();
						field_type = view.subfield_info->type;
					}

					publish_if_changed(root + "/state/" + field_path, view.field_ptr, field_type);

					if (should_publish_control && !view.struct_info->is_read_only())
					{
						const bool retain = true;
						const nlohmann::json payload = serialize_field(view.field_ptr, field_type);
						state->mqtt->publish(root + "/control/" + field_path, payload.dump(), retain);
					}
				});
		}

		void sync_updated_topics_from_mqtt()
		{
			WorkloadsBuffer& target_workloads_buffer =
				state->engine->get_workloads_buffer(); // sync directly info main buffer - needs careful thought...

			std::string control_prefix = config.root_topic_namespace.to_string() + "/control/";

			for (const auto& [topic, payload] : state->updated_topics)
			{
				if (topic.find(control_prefix) != 0)
					continue;

				// Strip off "robotick/control/"
				std::string suffix = topic.substr(control_prefix.size());

				// Split into path tokens
				std::vector<std::string> tokens;
				size_t start = 0, end;
				while ((end = suffix.find('/', start)) != std::string::npos)
				{
					tokens.push_back(suffix.substr(start, end - start));
					start = end + 1;
				}
				tokens.push_back(suffix.substr(start));

				if (tokens.size() < 3 || tokens.size() > 4)
					continue;

				const std::string& workload_name = tokens[0];
				const std::string& struct_name = tokens[1];
				const std::string& field_name = tokens[2];
				const std::string subfield_name = (tokens.size() == 4) ? tokens[3] : "";

				const WorkloadInstanceInfo* workload_info = state->engine->find_instance_info(workload_name.c_str());
				if (!workload_info)
				{
					ROBOTICK_WARNING("MqttClient - unable to find workload for incoming topic '%s'", topic.c_str());
					continue;
				}

				const StructRegistryEntry* struct_info = nullptr;
				if (struct_name == "config")
				{
					struct_info = workload_info->type->config_struct;
				}
				else if (struct_name == "inputs")
				{
					struct_info = workload_info->type->input_struct;
				}
				else if (struct_name == "outputs")
				{
					struct_info = workload_info->type->output_struct;
				}

				if (!struct_info)
				{
					ROBOTICK_WARNING("MqttClient - unable to find '%s' on workload for incoming topic '%s'", struct_name.c_str(), topic.c_str());
					continue;
				}

				const FieldInfo* field_info = struct_info->find_field(field_name.c_str());
				if (!field_info)
				{
					ROBOTICK_WARNING("MqttClient - unable to find field '%s' on workload for incoming topic '%s'", field_name.c_str(), topic.c_str());
					continue;
				}

				void* field_ptr = field_info->get_data_ptr(target_workloads_buffer, *workload_info, *struct_info);
				if (!field_ptr)
				{
					ROBOTICK_WARNING(
						"MqttClient - unable to find data-ptr for field '%s' on workload for incoming topic '%s'", field_name.c_str(), topic.c_str());
					continue;
				}

				TypeId field_type = field_info->type;

				if (field_type == GET_TYPE_ID(Blackboard))
				{
					Blackboard& blackboard = *static_cast<Blackboard*>(field_ptr);
					const BlackboardFieldInfo* sub_field_info = blackboard.get_field_info(subfield_name);
					if (!sub_field_info)
					{
						ROBOTICK_WARNING(
							"MqttClient - unable to find sub-field '%s' on workload for incoming topic '%s'", subfield_name.c_str(), topic.c_str());
					}

					field_ptr = sub_field_info->get_data_ptr(blackboard);
					field_type = sub_field_info->type;
				}

				try
				{
					if (field_type == GET_TYPE_ID(int))
						*reinterpret_cast<int*>(field_ptr) = payload.get<int>();
					else if (field_type == GET_TYPE_ID(double))
						*reinterpret_cast<double*>(field_ptr) = payload.get<double>();
					else if (field_type == GET_TYPE_ID(FixedString64))
						*reinterpret_cast<FixedString64*>(field_ptr) = FixedString64(payload.get<std::string>().c_str());
					else if (field_type == GET_TYPE_ID(FixedString128))
						*reinterpret_cast<FixedString128*>(field_ptr) = FixedString128(payload.get<std::string>().c_str());
					else
					{
						ROBOTICK_WARNING("MqttClient: Unsupported field type in topic %s", topic.c_str());
					}
				}
				catch (...)
				{
					ROBOTICK_WARNING("MqttClient: Failed to apply update from topic %s", topic.c_str());
				}
			}

			state->updated_topics.clear();
		}

		nlohmann::json serialize_field(void* field_ptr, const TypeId& field_type)
		{
			if (field_type == GET_TYPE_ID(int))
				return *reinterpret_cast<int*>(field_ptr);
			if (field_type == GET_TYPE_ID(double))
				return *reinterpret_cast<double*>(field_ptr);
			if (field_type == GET_TYPE_ID(FixedString64))
				return reinterpret_cast<FixedString64*>(field_ptr)->c_str();
			if (field_type == GET_TYPE_ID(FixedString128))
				return reinterpret_cast<FixedString128*>(field_ptr)->c_str();
			return nullptr;
		}

		void publish_if_changed(const std::string& topic, void* field_ptr, const TypeId& field_type)
		{
			nlohmann::json payload = serialize_field(field_ptr, field_type);
			if (state->last_published[topic] != payload)
			{
				state->mqtt->publish(topic, payload.dump(), true);
				state->last_published[topic] = payload;
			}
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(MqttClientWorkload, MqttClientConfig, MqttClientInputs, MqttClientOutputs)

} // namespace robotick
