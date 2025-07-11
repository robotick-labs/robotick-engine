// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#if defined(ROBOTICK_PLATFORM_DESKTOP)

#include "robotick/framework/data/MqttFieldSync.h"
#include "robotick/api.h"

namespace robotick
{
	// Constructor for tests: only publisher lambda
	MqttFieldSync::MqttFieldSync(const std::string& root_ns, PublisherFn publisher)
		: root(root_ns)
		, publisher(std::move(publisher))
		, mqtt_ptr(nullptr)
		, engine_ptr(nullptr)
	{
	}

	// Constructor for real use: Engine + root namespace + IMqttClient
	MqttFieldSync::MqttFieldSync(Engine& engine, const std::string& root_ns, IMqttClient& mqtt_client)
		: root(root_ns)
		, publisher(nullptr)
		, mqtt_ptr(&mqtt_client)
		, engine_ptr(&engine)
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

		const std::string prefix = root + "/control/";

		for (const auto& [topic, json_value] : updated_topics)
		{
			// Skip unrelated topics
			if (topic.find(prefix) != 0)
				continue;

			// Convert MQTT topic to dot-delimited field path
			std::string path = topic.substr(prefix.size());
			std::replace(path.begin(), path.end(), '/', '.');

			// Try to resolve the field
			auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine_ptr, path.c_str());

			if (ptr == nullptr)
			{
				ROBOTICK_WARNING("MqttFieldSync::apply_control_updates() - unable to resolve field path: %s", path.c_str());
				continue;
			}

			if (field_desc == nullptr)
			{
				ROBOTICK_WARNING("MqttFieldSync::apply_control_updates() - resolved pointer but missing field descriptor for: %s", path.c_str());
				continue;
			}

			const TypeDescriptor* type_desc = field_desc->find_type_descriptor();
			if (type_desc == nullptr)
			{
				ROBOTICK_WARNING("MqttFieldSync::apply_control_updates() - field '%s' has no registered type descriptor", path.c_str());
				continue;
			}

			// Parse and assign value
			const std::string value_str = json_value.dump();
			if (!type_desc->from_string(value_str.c_str(), ptr))
			{
				ROBOTICK_WARNING(
					"MqttFieldSync::apply_control_updates() - failed to parse value '%s' for field '%s'", value_str.c_str(), path.c_str());
				continue;
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

		WorkloadFieldsIterator::for_each_workload_field(engine,
			&non_const_buf,
			[&](const WorkloadFieldView& view)
			{
				if (!view.workload_info || !view.struct_info || !view.field_info)
					return;

				const WorkloadDescriptor* workload_desc = view.workload_info->type->get_workload_desc();
				if (!workload_desc)
					return;

				// Build path: "W/struct/field[/subfield]"
				bool is_struct_read_only = true;
				std::string struct_name;
				if (view.struct_info == workload_desc->config_desc)
				{
					struct_name = "config";
				}
				else if (view.struct_info == workload_desc->inputs_desc)
				{
					struct_name = "inputs";
					is_struct_read_only = false;
				}
				else if (view.struct_info == workload_desc->outputs_desc)
				{
					struct_name = "outputs";
				}
				else
				{
					return;
				}

				std::string path = std::string(view.workload_info->seed->unique_name) + "/" + struct_name + "/" + std::string(view.field_info->name);
				TypeId type = view.field_info->type_id;
				if (view.subfield_info)
				{
					path += "/" + std::string(view.subfield_info->name.c_str());
					type = view.subfield_info->type_id;
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

				if (publish_control && !is_struct_read_only)
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

#endif // #if defined(ROBOTICK_PLATFORM_DESKTOP)
