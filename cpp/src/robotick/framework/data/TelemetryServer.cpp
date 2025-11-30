// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/strings/StringView.h"
#include "robotick/framework/utility/Algorithm.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"
#include "robotick/framework/time/Clock.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <unistd.h>

#if defined(ROBOTICK_PLATFORM_ESP32S3)
#include <esp_random.h>
#endif

namespace robotick
{

	TelemetryServer::~TelemetryServer()
	{
		stop();
	}

	static void build_session_id(const char* model_name, FixedString64& session_id)
	{
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		uint32_t raw = esp_random(); // 32-bit hardware RNG
		session_id.format("%08X-%s", raw, model_name);
#elif defined(ROBOTICK_PLATFORM_DESKTOP)
		const uint64_t raw = static_cast<uint64_t>(Clock::to_nanoseconds(Clock::now().time_since_epoch()).count()); // 64-bit timestamp
		session_id.format("%016llX-%s", static_cast<unsigned long long>(raw), model_name);
#else
		session_id.format("12345-%s", model_name);
#endif
	}

	void TelemetryServer::start(const Engine& engine_in, const uint16_t telemetry_port)
	{
		engine = &engine_in;

		build_session_id(engine_in.get_model_name(), session_id);

		web_server.start("Telemetry",
			telemetry_port,
			nullptr,
			[this](const WebRequest& req, WebResponse& res)
			{
				if (!req.method.equals("GET"))
				{
					return false; // not handled
				}

				if (!engine)
				{
					res.set_content_type("text/plain");
					res.set_status_code(WebResponseCode::InternalServerError);
					res.set_body_string("{\"error\":\"no engine available\"}");
					return true;
				}

				if (req.uri.equals("/api/telemetry/health"))
				{
					res.set_status_code(WebResponseCode::OK);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workloads_buffer/layout"))
				{
					handle_get_workloads_buffer_layout(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workloads_buffer/raw"))
				{
					handle_get_workloads_buffer_raw(req, res);
					return true;
				}

				return false;
			});
	}

	void TelemetryServer::stop()
	{
		web_server.stop();
	}

	static bool type_already_emitted(const nlohmann::ordered_json& layout_json, const char* name)
	{
		const auto& types_array = layout_json["types"];
		for (const auto& type_json : types_array)
		{
			if (type_json.contains("name") && type_json["name"] == name)
			{
				return true;
			}
		}
		return false;
	}

	static FixedString64 make_blackboard_type_name(const DynamicStructDescriptor& desc, void* data_ptr)
	{
		const StructDescriptor* struct_desc = desc.get_struct_descriptor(data_ptr);
		robotick::Hash32 hash;

		if (struct_desc)
		{
			for (const FieldDescriptor& field : struct_desc->fields)
			{
				hash.update_cstring(field.name.c_str());
				hash.update(field.type_id.value);
				hash.update(field.offset_within_container);
				hash.update(field.element_count);
			}
		}

		FixedString64 type_name;
		type_name.format("Blackboard_%08X", static_cast<unsigned int>(hash.final()));
		return type_name;
	}

	static FixedString256 get_type_name(const TypeDescriptor& type_desc, void* data_ptr)
	{
		const DynamicStructDescriptor* dynamic_struct_desc = type_desc.get_dynamic_struct_desc();
		if (dynamic_struct_desc)
		{
			const FixedString64 blackboard_name = make_blackboard_type_name(*dynamic_struct_desc, data_ptr);
			FixedString256 type_name;
			type_name = blackboard_name.c_str();
			return type_name;
		}

		FixedString256 type_name;
		if (!type_desc.name.empty())
		{
			type_name = type_desc.name.c_str();
		}
		else
		{
			type_name = "unknown";
		}
		return type_name;
	}

	static void emit_type_info(
		nlohmann::ordered_json& layout_json, const WorkloadsBuffer& workloads_buffer, void* data_ptr, const TypeDescriptor* type_desc)
	{
		if (!type_desc)
		{
			return;
		}

		const FixedString256 type_name = get_type_name(*type_desc, data_ptr);
		if (type_already_emitted(layout_json, type_name.c_str()))
		{
			return;
		}

		nlohmann::ordered_json type_json;
		type_json["name"] = type_name.c_str();
		type_json["size"] = type_desc->size;
		type_json["alignment"] = type_desc->alignment;
		type_json["type_category"] = type_desc->type_category;

		if (!type_desc->mime_type.empty())
		{
			type_json["mime_type"] = type_desc->mime_type.c_str();
		}

		const DynamicStructDescriptor* dynamic_struct_desc = type_desc->get_dynamic_struct_desc();
		const StructDescriptor* struct_desc =
			dynamic_struct_desc ? dynamic_struct_desc->get_struct_descriptor(data_ptr) : type_desc->get_struct_desc();

		if (struct_desc)
		{
			type_json["fields"] = nlohmann::ordered_json::array();

			for (const FieldDescriptor& field_desc : struct_desc->fields)
			{
				const TypeDescriptor* field_type = field_desc.find_type_descriptor();
				if (!field_type)
				{
					continue;
				}

				void* field_data_ptr = field_desc.get_data_ptr(data_ptr);

				ROBOTICK_ASSERT(workloads_buffer.contains_object_used_space(field_data_ptr, field_type->size));

				nlohmann::ordered_json field_json;
				field_json["name"] = field_desc.name;
				field_json["offset_within_container"] = static_cast<int>(field_desc.offset_within_container);
				field_json["type"] = get_type_name(*field_type, field_data_ptr);
				field_json["element_count"] = static_cast<int>(field_desc.element_count);

				type_json["fields"].push_back(field_json);

				emit_type_info(layout_json, workloads_buffer, field_data_ptr, field_type);
			}
		}

		layout_json["types"].push_back(type_json);
	}

	static void emit_struct_info(nlohmann::ordered_json& layout_json,
		nlohmann::ordered_json& workload_json,
		const WorkloadsBuffer& workloads_buffer,
		void* workload_ptr,
		const char* struct_name,
		const TypeDescriptor* struct_desc,
		const size_t base_offset)
	{
		if (!struct_desc || !workload_ptr)
		{
			return;
		}

		nlohmann::ordered_json struct_json;
		struct_json["type"] = struct_desc ? struct_desc->name.c_str() : "null";
		struct_json["offset_within_container"] = static_cast<int>(base_offset);

		workload_json[struct_name] = struct_json;

		void* data_ptr = (uint8_t*)workload_ptr + base_offset;

		emit_type_info(layout_json, workloads_buffer, data_ptr, struct_desc);
	}

	static size_t get_process_memory_used()
	{
		long rss_pages = 0;
		long total_pages = 0;
		FILE* statm = ::fopen("/proc/self/statm", "r");

		if (statm)
		{
			if (::fscanf(statm, "%ld %ld", &total_pages, &rss_pages) == 2)
			{
				const long page_size = ::sysconf(_SC_PAGESIZE);
				::fclose(statm);
				return static_cast<size_t>(rss_pages) * page_size;
			}
			::fclose(statm);
		}

		return 0;
	}

	void TelemetryServer::handle_get_workloads_buffer_layout(const WebRequest& /*req*/, WebResponse& res)
	{
		WorkloadsBuffer& workloads_buffer = engine->get_workloads_buffer();
		const auto& instances = engine->get_all_instance_info();

		nlohmann::ordered_json layout_json;
		layout_json["engine_session_id"] = get_session_id();
		layout_json["workloads_buffer_size_used"] = workloads_buffer.get_size_used();
		layout_json["process_memory_used"] = get_process_memory_used();
		layout_json["workloads"] = nlohmann::ordered_json::array();
		layout_json["types"] = nlohmann::ordered_json::array();

		// lookup type for workload-stats
		const auto* workload_stats_type = TypeRegistry::get().find_by_id(GET_TYPE_ID(WorkloadInstanceStats));
		ROBOTICK_ASSERT_MSG(workload_stats_type, "Type 'WorkloadInstanceStats' not registered - this should never happen");

		for (const WorkloadInstanceInfo& workload_instance_info : instances)
		{
			if (workload_instance_info.seed == nullptr || workload_instance_info.type == nullptr)
			{
				ROBOTICK_WARNING("TelemetryServer: Null workload seed or type, skipping...");
				continue;
			}

			const WorkloadDescriptor* desc = workload_instance_info.type->get_workload_desc();
			if (desc == nullptr)
			{
				ROBOTICK_WARNING(
					"TelemetryServer: WorkloadDescriptor is null for '%s', skipping...", workload_instance_info.seed->unique_name.c_str());
				continue;
			}

			nlohmann::ordered_json workload_json;
			workload_json["name"] = workload_instance_info.seed->unique_name;
			workload_json["type"] = workload_instance_info.type->name.c_str();
			workload_json["offset_within_container"] = static_cast<int>(workload_instance_info.offset_in_workloads_buffer);

			void* workload_ptr = (void*)workload_instance_info.get_ptr(workloads_buffer);

			emit_struct_info(layout_json, workload_json, workloads_buffer, workload_ptr, "config", desc->config_desc, desc->config_offset);
			emit_struct_info(layout_json, workload_json, workloads_buffer, workload_ptr, "inputs", desc->inputs_desc, desc->inputs_offset);
			emit_struct_info(layout_json, workload_json, workloads_buffer, workload_ptr, "outputs", desc->outputs_desc, desc->outputs_offset);

			// emits stats info:
			workload_json["stats_offset_within_container"] =
				static_cast<int>((uint8_t*)workload_instance_info.workload_stats - workloads_buffer.raw_ptr());

			emit_type_info(layout_json, workloads_buffer, workload_instance_info.workload_stats, workload_stats_type);

			layout_json["workloads"].push_back(workload_json);
		}

		// sort workloads by offset (so we're seeing them in true memory-layout order)
		robotick::sort(layout_json["workloads"].begin(),
			layout_json["workloads"].end(),
			[](const nlohmann::ordered_json& a, const nlohmann::ordered_json& b)
			{
				return a["offset_within_container"].get<int>() < b["offset_within_container"].get<int>();
			});

		robotick::sort(layout_json["types"].begin(),
			layout_json["types"].end(),
			[](const nlohmann::ordered_json& a, const nlohmann::ordered_json& b)
			{
				const auto& a_name = a["name"].get_ref<const nlohmann::ordered_json::string_t&>();
				const auto& b_name = b["name"].get_ref<const nlohmann::ordered_json::string_t&>();
				return StringView(a_name.c_str()) < StringView(b_name.c_str());
			});

		res.set_status_code(WebResponseCode::OK);
		res.set_content_type("application/json");

		auto out_str = layout_json.dump(2); // Pretty print
		res.set_body_string(out_str.c_str());
	}

	void TelemetryServer::handle_get_workloads_buffer_raw(const WebRequest& /*req*/, WebResponse& res)
	{
		const WorkloadsBuffer& workloads_buffer = engine->get_workloads_buffer();

		res.set_status_code(WebResponseCode::OK);
		res.set_content_type("application/octet-stream");

		// Attach the session ID as a custom response header
		FixedString256 session_header;
		session_header.format("X-Robotick-Session-Id:%s", get_session_id());
		res.add_header(session_header.c_str());

		res.set_body(workloads_buffer.raw_ptr(), workloads_buffer.get_size_used());
	}

} // namespace robotick
