// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/MqttFieldSync.h"
#include "robotick/api.h"

namespace robotick
{
	// Constructor for tests: only publisher lambda
	MqttFieldSync::MqttFieldSync(const std::string& root_ns, PublisherFn publisher)
		: root(root_ns), publisher(std::move(publisher)), mqtt_ptr(nullptr), engine_ptr(nullptr)
	{
	}

	// Constructor for real use: Engine + root namespace + IMqttClient
	MqttFieldSync::MqttFieldSync(Engine& engine, const std::string& root_ns, IMqttClient& mqtt_client)
		: root(root_ns), publisher(nullptr), mqtt_ptr(&mqtt_client), engine_ptr(&engine)
	{
		try
		{

			// Register callback on the IMqttClient
			mqtt_ptr->set_callback(
				[this](const std::string& topic, const std::string& payload)
				{
					// Only care about "<root>/control/…"
					if (topic.find(root + "/control/") != 0)
						return;

					nlohmann::json incoming;
					try
					{
						incoming = nlohmann::json::parse(payload);
					}
					catch (...)
					{
						ROBOTICK_WARNING("MqttFieldSync - Ignoring malformed JSON from topic: %s", topic.c_str());
						return;
					}

					auto it = last_published.find(topic);
					if (it != last_published.end() && it->second == incoming)
						return;

					updated_topics[topic] = incoming;
				});
		}
		catch (...)
		{
			ROBOTICK_FATAL_EXIT("MqttFieldSync - Failed to set MQTT callback.");
		}
	}

	// Subscribe to control/# and publish initial state+control
	void MqttFieldSync::subscribe_and_sync_startup()
	{
		ROBOTICK_ASSERT_MSG(mqtt_ptr != nullptr, "MqttFieldSync::subscribe_and_sync_startup - mqtt_ptr should have been set before calling");
		ROBOTICK_ASSERT_MSG(engine_ptr != nullptr, "MqttFieldSync::subscribe_and_sync_startup - engine_ptr should have been set before calling");

		try
		{
			mqtt_ptr->subscribe(root + "/control/#");
		}
		catch (...)
		{
			ROBOTICK_WARNING("MqttFieldSync - Failed to subscribe to control topics.");
		}

		try
		{
			publish_fields(*engine_ptr, engine_ptr->get_workloads_buffer(), true);
		}
		catch (...)
		{
			ROBOTICK_WARNING("MqttFieldSync - Failed to publish startup fields.");
		}

		updated_topics.clear();
	}

	// Apply any queued control updates into the Engine’s main buffer
	void MqttFieldSync::apply_control_updates()
	{
		if (!engine_ptr)
			return;

		WorkloadsBuffer& buffer = engine_ptr->get_workloads_buffer();
		const std::string prefix = root + "/control/";

		for (const auto& [topic, json_value] : updated_topics)
		{
			if (topic.find(prefix) != 0)
				continue;

			std::string suffix = topic.substr(prefix.size());
			// Split "W/inputs/field[/subfield]" into tokens
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

			const std::string& wname = tokens[0];
			const std::string& sname = tokens[1];
			const std::string& fname = tokens[2];
			const std::string sfield = (tokens.size() == 4 ? tokens[3] : "");

			// Find the workload instance
			const WorkloadInstanceInfo* winfo = engine_ptr->find_instance_info(wname.c_str());
			if (!winfo)
			{
				ROBOTICK_WARNING("MqttFieldSync - unknown workload %s", topic.c_str());
				continue;
			}

			// Determine which struct (config/inputs/outputs)
			const StructRegistryEntry* sinfo = nullptr;
			if (sname == "config")
				sinfo = winfo->type->config_struct;
			else if (sname == "inputs")
				sinfo = winfo->type->input_struct;
			else if (sname == "outputs")
				sinfo = winfo->type->output_struct;

			if (!sinfo)
			{
				ROBOTICK_WARNING("MqttFieldSync - unknown struct %s", topic.c_str());
				continue;
			}

			// Find the FieldInfo
			const FieldInfo* finfo = sinfo->find_field(fname.c_str());
			if (!finfo)
			{
				ROBOTICK_WARNING("MqttFieldSync - unknown field %s", topic.c_str());
				continue;
			}

			// Get pointer to that field
			void* ptr = finfo->get_data_ptr(buffer, *winfo, *sinfo);
			if (!ptr)
			{
				ROBOTICK_WARNING("MqttFieldSync - data ptr missing %s", topic.c_str());
				continue;
			}

			// Handle Blackboard subfields if needed
			TypeId type = finfo->type;
			if (type == GET_TYPE_ID(Blackboard))
			{
				Blackboard& bb = *static_cast<Blackboard*>(ptr);
				const BlackboardFieldInfo* sbinfo = bb.get_field_info(sfield);
				if (!sbinfo)
				{
					ROBOTICK_WARNING("MqttFieldSync - unknown subfield %s", topic.c_str());
					continue;
				}
				ptr = sbinfo->get_data_ptr(bb);
				type = sbinfo->type;
			}

			// Apply the JSON value to that pointer
			try
			{
				if (type == GET_TYPE_ID(int))
					*reinterpret_cast<int*>(ptr) = json_value.get<int>();
				else if (type == GET_TYPE_ID(double))
					*reinterpret_cast<double*>(ptr) = json_value.get<double>();
				else if (type == GET_TYPE_ID(FixedString64))
					*reinterpret_cast<FixedString64*>(ptr) = FixedString64(json_value.get<std::string>().c_str());
				else if (type == GET_TYPE_ID(FixedString128))
					*reinterpret_cast<FixedString128*>(ptr) = FixedString128(json_value.get<std::string>().c_str());
				else
				{
					ROBOTICK_WARNING("MqttFieldSync - unsupported type %s", topic.c_str());
				}
			}
			catch (...)
			{
				ROBOTICK_WARNING("MqttFieldSync - apply failed %s", topic.c_str());
			}
		}

		updated_topics.clear();
	}

	// Publish only state fields each tick (no control)
	void MqttFieldSync::publish_state_fields()
	{
		if (!engine_ptr)
			return;
		publish_fields(*engine_ptr, engine_ptr->get_workloads_buffer(), false);
	}

	// Publish all fields under "<root>/state/..." and optionally "<root>/control/..."
	void MqttFieldSync::publish_fields(const Engine& engine, const WorkloadsBuffer& buffer, bool publish_control)
	{
		// WorkloadFieldsIterator currently requires non-const buffer
		WorkloadsBuffer& non_const_buf = const_cast<WorkloadsBuffer&>(buffer);

		WorkloadFieldsIterator::for_each_workload_field(engine, &non_const_buf,
			[&](const WorkloadFieldView& view)
			{
				if (!view.workload_info || !view.struct_info || !view.field_info)
					return;

				// Build path: "W/struct/field[/subfield]"
				std::string path = view.workload_info->unique_name + "/" + view.struct_info->local_name + "/" + view.field_info->name;
				TypeId type = view.field_info->type;
				if (view.subfield_info)
				{
					path += "/" + view.subfield_info->name.to_string();
					type = view.subfield_info->type;
				}

				// Serialize to JSON
				nlohmann::json value = serialize(view.field_ptr, type);

				// Publish "<root>/state/<path>"
				const std::string state_topic = root + "/state/" + path;
				last_published[state_topic] = value;

				try
				{
					if (mqtt_ptr)
						mqtt_ptr->publish(state_topic, value.dump(), true);
					else if (publisher)
						publisher("state/" + path, value.dump(), true);
				}
				catch (...)
				{
					ROBOTICK_WARNING("MqttFieldSync - Failed to publish state topic: %s", state_topic.c_str());
				}

				// If requested, also publish "<root>/control/<path>" for writable fields
				const std::string control_topic = root + "/control/" + path;

				if (publish_control && !view.struct_info->is_read_only())
				{
					last_published[control_topic] = value;
					try
					{
						if (mqtt_ptr)
							mqtt_ptr->publish(control_topic, value.dump(), true);
						else if (publisher)
							publisher("control/" + path, value.dump(), true);
					}
					catch (...)
					{
						ROBOTICK_WARNING("MqttFieldSync - Failed to publish control topic: %s", control_topic.c_str());
					}
				}
			});
	}

	// Helper: serialize a field pointer by TypeId into JSON
	nlohmann::json MqttFieldSync::serialize(void* ptr, TypeId type)
	{
		if (type == GET_TYPE_ID(int))
			return *reinterpret_cast<int*>(ptr);
		if (type == GET_TYPE_ID(double))
			return *reinterpret_cast<double*>(ptr);
		if (type == GET_TYPE_ID(FixedString64))
			return reinterpret_cast<FixedString64*>(ptr)->c_str();
		if (type == GET_TYPE_ID(FixedString128))
			return reinterpret_cast<FixedString128*>(ptr)->c_str();
		return nullptr;
	}
} // namespace robotick
