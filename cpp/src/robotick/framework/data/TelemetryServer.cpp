#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include <nlohmann/json.hpp>

namespace robotick
{
	static void build_session_id(const char* model_name, FixedString64& session_id)
	{
#if defined(ROBOTICK_PLATFORM_ESP32)
		uint32_t raw = esp_random(); // 32-bit hardware RNG
		session_id.format("%08X-%s", raw, model_name);
#elif defined(ROBOTICK_PLATFORM_DESKTOP)
		uint64_t raw = std::chrono::steady_clock::now().time_since_epoch().count(); // 64-bit timestamp
		session_id.format("%016llX-%s", static_cast<unsigned long long>(raw), model_name);
#else
		uint32_t raw = 0xDEADBEEF;
		session_id.format("%08X-%s", raw, model_name);
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
					res.status_code = 404;
					return true;
				}

				if (!engine)
				{
					res.body.set_from_string("{\"error\":\"no engine available\"}");
					res.status_code = 500;
					return true;
				}

				if (req.uri.equals("/api/telemetry/workloads"))
				{
					handle_get_workloads(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/stats"))
				{
					handle_get_workload_stats(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/config"))
				{
					handle_get_workload_config(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/inputs"))
				{
					handle_get_workload_inputs(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/outputs"))
				{
					handle_get_workload_outputs(req, res);
					return true;
				}
				else if (req.uri.equals("/api/telemetry/workload/output_png"))
				{
					handle_get_workload_output_png(req, res);
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

	static void emit_type_info(nlohmann::ordered_json& layout_json, const TypeDescriptor* type_desc)
	{
		if (!type_desc || type_already_emitted(layout_json, type_desc->name.c_str()))
		{
			return;
		}

		nlohmann::ordered_json type_json;
		type_json["name"] = type_desc->name.c_str();
		type_json["size"] = type_desc->size;
		type_json["alignment"] = type_desc->alignment;
		type_json["type_category"] = type_desc->type_category;

		if (!type_desc->meta.empty())
		{
			type_json["meta"] = type_desc->meta.c_str();
		}

		const void* blackboard_instance = nullptr; // this will disable these for now, until we're ready to add support for blackboards

		const DynamicStructDescriptor* dynamic_struct_desc = type_desc->get_dynamic_struct_desc();
		const StructDescriptor* struct_desc = dynamic_struct_desc && blackboard_instance
												  ? dynamic_struct_desc->get_struct_descriptor(blackboard_instance)
												  : type_desc->get_struct_desc();
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

				nlohmann::ordered_json field_json;
				field_json["name"] = field_desc.name;
				field_json["offset_within_container"] = static_cast<int>(field_desc.offset_within_container);
				field_json["type"] = field_type->name.c_str();

				type_json["fields"].push_back(field_json);

				emit_type_info(layout_json, field_type);
			}
		}

		layout_json["types"].push_back(type_json);
	}

	static void emit_struct_info(nlohmann::ordered_json& layout_json,
		nlohmann::ordered_json& workload_json,
		const char* struct_name,
		const TypeDescriptor* struct_desc,
		const size_t base_offset)
	{
		if (!struct_desc)
		{
			return;
		}

		nlohmann::ordered_json struct_json;
		struct_json["type"] = struct_desc ? struct_desc->name.c_str() : "null";
		struct_json["offset_within_container"] = static_cast<int>(base_offset);

		workload_json[struct_name].push_back(struct_json);

		emit_type_info(layout_json, struct_desc);
	}

	void TelemetryServer::handle_get_workloads_buffer_layout(const WebRequest& /*req*/, WebResponse& res)
	{
		WorkloadsBuffer& buffer = engine->get_workloads_buffer();
		const auto& instances = engine->get_all_instance_info();

		nlohmann::ordered_json layout_json;
		layout_json["engine_session_id"] = get_session_id();
		layout_json["total_size"] = buffer.get_size_used();
		layout_json["workloads"] = nlohmann::ordered_json::array();
		layout_json["types"] = nlohmann::ordered_json::array();

		for (const WorkloadInstanceInfo& info : instances)
		{
			if (info.seed == nullptr || info.type == nullptr)
			{
				ROBOTICK_WARNING("TelemetryServer: Null workload seed or type, skipping...");
				continue;
			}

			const WorkloadDescriptor* desc = info.type->get_workload_desc();
			if (desc == nullptr)
			{
				ROBOTICK_WARNING("TelemetryServer: WorkloadDescriptor is null for '%s', skipping...", info.seed->unique_name.c_str());
				continue;
			}

			nlohmann::ordered_json workload_json;
			workload_json["name"] = info.seed->unique_name;
			workload_json["type"] = info.type->name.c_str();
			workload_json["offset_within_container"] = static_cast<int>(info.offset_in_workloads_buffer);

			emit_struct_info(layout_json, workload_json, "config", desc->config_desc, desc->config_offset);
			emit_struct_info(layout_json, workload_json, "inputs", desc->inputs_desc, desc->inputs_offset);
			emit_struct_info(layout_json, workload_json, "outputs", desc->outputs_desc, desc->outputs_offset);

			layout_json["workloads"].push_back(workload_json);
		}

		// sort workloads by offset (so we're seeing them in true memory-layout order)
		std::sort(layout_json["workloads"].begin(),
			layout_json["workloads"].end(),
			[](const nlohmann::ordered_json& a, const nlohmann::ordered_json& b)
			{
				return a["offset_within_container"].get<int>() < b["offset_within_container"].get<int>();
			});

		std::sort(layout_json["types"].begin(),
			layout_json["types"].end(),
			[](const nlohmann::ordered_json& a, const nlohmann::ordered_json& b)
			{
				return a["name"].get<std::string>() < b["name"].get<std::string>();
			});

		std::string out_str = layout_json.dump(2); // Pretty print
		res.body.set_from_string(out_str.c_str());
		res.status_code = WebResponseCode::OK;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workloads_buffer_raw(const WebRequest& /*req*/, WebResponse& res)
	{
		const WorkloadsBuffer& workloads_buffer = engine->get_workloads_buffer();

		res.body.set_bytes(workloads_buffer.raw_ptr(), workloads_buffer.get_size_used());
		res.status_code = WebResponseCode::OK;
		res.content_type = "application/octet-stream";

		// Attach the session ID as a custom response header
		FixedString256 session_header;
		session_header.format("X-Robotick-Session-ID:%s", get_session_id());
		res.headers.add(session_header);
	}

	void TelemetryServer::handle_get_workloads(const WebRequest& /*req*/, WebResponse& res)
	{
		const auto& instances = engine->get_all_instance_info();
		res.body.clear();
		res.body.append_from_string("{\"workloads\":[");

		bool first = true;
		for (const auto& info : instances)
		{
			if (!first)
			{
				res.body.append_from_string(",");
			}
			first = false;
			res.body.append_from_string_format("{\"name\":\"%s\",\"type\":\"%s\"}", info.seed->unique_name.c_str(), info.type->name.c_str());
		}

		res.body.append_from_string("]}");
		res.status_code = WebResponseCode::OK;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workload_stats(const WebRequest& req, WebResponse& res)
	{
		const char* workload_unique_name = req.find_query_param("name");
		const WorkloadInstanceInfo* info = workload_unique_name != nullptr ? engine->find_instance_info(workload_unique_name) : nullptr;
		if (!info)
		{
			res.body.set_from_string("{\"error\":\"not found\"}");
			res.status_code = 404;
			return;
		}

		res.body.clear();
		res.body.append_from_string_format("{\"self_ms\":%f,\"dt_ms\":%f,\"goal_ms\":%f}",
			info->mutable_stats.get_last_tick_duration_ms(),
			info->mutable_stats.get_last_time_delta_ms(),
			(info->seed->tick_rate_hz > 0.0) ? (1000.0f / info->seed->tick_rate_hz) : -1.0f);

		res.status_code = WebResponseCode::OK;
		res.content_type = "application/json";
	}

	// ---- Helper: JSON-escape a C-string into `out` (no STL, no exceptions)
	static void json_escape_and_write(WebResponseBodyBuffer& out, const char* s)
	{
		out.append_from_string("\"");
		for (const char* p = s; *p; ++p)
		{
			const char ch = *p;
			switch (ch)
			{
			case '\"':
				out.append_from_string("\\\"");
				break;
			case '\\':
				out.append_from_string("\\\\");
				break;
			case '\b':
				out.append_from_string("\\b");
				break;
			case '\f':
				out.append_from_string("\\f");
				break;
			case '\n':
				out.append_from_string("\\n");
				break;
			case '\r':
				out.append_from_string("\\r");
				break;
			case '\t':
				out.append_from_string("\\t");
				break;
			default:
				if ((unsigned char)ch < 0x20)
				{
					// \u00XX
					char buf[7];
					const char* hex = "0123456789ABCDEF";
					unsigned char v = (unsigned char)ch;
					buf[0] = '\\';
					buf[1] = 'u';
					buf[2] = '0';
					buf[3] = '0';
					buf[4] = hex[(v >> 4) & 0xF];
					buf[5] = hex[v & 0xF];
					buf[6] = '\0';
					out.append_from_string(buf);
				}
				else
				{
					char one[2] = {ch, '\0'};
					out.append_from_string(one);
				}
				break;
			}
		}
		out.append_from_string("\"");
	}

	// ---- Recursively emit a JSON object for a nested struct field (non-templated)
	static void emit_struct_object_field(WebResponseBodyBuffer& out, WorkloadsBuffer& buffer, const WorkloadFieldView& parent)
	{
		out.append_from_string("{");
		bool first = true;

		WorkloadFieldsIterator::for_each_field_in_struct_field(parent,
			[&](const WorkloadFieldView& view)
			{
				if (!first)
					out.append_from_string(",");
				first = false;

				// Key
				json_escape_and_write(out, view.field_info->name.c_str());
				out.append_from_string(":");

				if (view.is_struct_field())
				{
					// Recurse
					emit_struct_object_field(out, buffer, view);
				}
				else
				{
					// Leaf via TypeDescriptor::to_string
					const TypeDescriptor* td = view.field_info->find_type_descriptor();

					FixedString256 value = "<invalid>";
					if (td && td->to_string && view.field_ptr && buffer.contains_object(view.field_ptr, td->size))
					{
						if (!td->to_string(view.field_ptr, value.data, value.capacity()))
							value.format("<%s>", td->name);
					}
					json_escape_and_write(out, value.c_str());
				}
			});

		out.append_from_string("}");
	}

	// ---- Emit the top-level struct object (non-templated)
	static void emit_struct_object_root(WebResponseBodyBuffer& out,
		WorkloadsBuffer& buffer,
		const WorkloadInstanceInfo& info,
		const TypeDescriptor* struct_desc,
		size_t struct_offset)
	{
		out.append_from_string("{");
		bool first = true;

		WorkloadFieldsIterator::for_each_field_in_struct(info,
			struct_desc,
			struct_offset,
			buffer,
			[&](const WorkloadFieldView& view)
			{
				if (!first)
					out.append_from_string(",");
				first = false;

				// Key
				json_escape_and_write(out, view.field_info->name.c_str());
				out.append_from_string(":");

				if (view.is_struct_field())
				{
					emit_struct_object_field(out, buffer, view);
				}
				else
				{
					// Leaf via TypeDescriptor::to_string
					const TypeDescriptor* td = view.field_info->find_type_descriptor();

					FixedString256 value = "<invalid>";
					if (td && td->to_string && view.field_ptr && buffer.contains_object(view.field_ptr, td->size))
					{
						if (!td->to_string(view.field_ptr, value.data, value.capacity()))
							value.format("<%s>", td->name);
					}
					json_escape_and_write(out, value.c_str());
				}
			});

		out.append_from_string("}");
	}

	// ---- Entry point
	static void emit_fields_as_json(WebResponseBodyBuffer& out,
		const WorkloadInstanceInfo& info,
		const TypeDescriptor* struct_desc,
		size_t struct_offset,
		WorkloadsBuffer& buffer)
	{
		emit_struct_object_root(out, buffer, info, struct_desc, struct_offset);
	}

	void TelemetryServer::handle_get_workload_config(const WebRequest& req, WebResponse& res)
	{
		const char* name = req.find_query_param("name");
		const WorkloadInstanceInfo* info = name ? engine->find_instance_info(name) : nullptr;

		if (!info)
		{
			res.body.set_from_string("{\"error\":\"not found\"}");
			res.status_code = 404;
			return;
		}

		WorkloadsBuffer& mirror = engine->get_workloads_buffer();
		res.body.clear();

		emit_fields_as_json(res.body, *info, info->type->get_workload_desc()->config_desc, info->type->get_workload_desc()->config_offset, mirror);

		res.status_code = WebResponseCode::OK;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workload_io(const WebRequest& req, WebResponse& res, bool inputs)
	{
		const char* name = req.find_query_param("name");
		const WorkloadInstanceInfo* info = name ? engine->find_instance_info(name) : nullptr;

		if (!info)
		{
			res.body.set_from_string("{\"error\":\"not found\"}");
			res.status_code = 404;
			return;
		}

		const WorkloadDescriptor* desc = info->type->get_workload_desc();
		const TypeDescriptor* struct_desc = inputs ? desc->inputs_desc : desc->outputs_desc;
		size_t struct_offset = inputs ? desc->inputs_offset : desc->outputs_offset;

		WorkloadsBuffer& mirror = engine->get_workloads_buffer();
		res.body.clear();

		emit_fields_as_json(res.body, *info, struct_desc, struct_offset, mirror);

		res.status_code = WebResponseCode::OK;
		res.content_type = "application/json";
	}

	void TelemetryServer::handle_get_workload_inputs(const WebRequest& req, WebResponse& res)
	{
		handle_get_workload_io(req, res, true);
	}

	void TelemetryServer::handle_get_workload_outputs(const WebRequest& req, WebResponse& res)
	{
		handle_get_workload_io(req, res, false);
	}

	void TelemetryServer::handle_get_workload_output_png(const WebRequest& req, WebResponse& res)
	{
		const char* workload_unique_name = req.find_query_param("name");
		const char* field_id = req.find_query_param("field");

		if (!workload_unique_name || !field_id)
		{
			res.body.set_from_string("{\"error\":\"missing query params: require name and field\"}");
			res.status_code = 400;
			res.content_type = "application/json";
			return;
		}

		// Construct full path: e.g., workload.outputs.my.field
		FixedString256 full_path;
		full_path.format("%s.outputs.%s", workload_unique_name, field_id);

		auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*engine, full_path.c_str());
		if (!ptr || !field_desc)
		{
			res.body.set_from_string("{\"error\":\"field not found in outputs\"}");
			res.status_code = 404;
			res.content_type = "application/json";
			return;
		}

		const uint8_t* obj = static_cast<const uint8_t*>(ptr);
		const size_t obj_size = size;

		auto is_png_sig = [](const uint8_t* p) -> bool
		{
			static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
			for (int i = 0; i < 8; i++)
				if (p[i] != sig[i])
					return false;
			return true;
		};

		auto be32 = [](const uint8_t* p) -> uint32_t
		{
			return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
		};

		// Locate PNG start
		size_t png_start = SIZE_MAX;
		for (size_t i = 0; i + 8 <= obj_size; ++i)
		{
			if (is_png_sig(obj + i))
			{
				png_start = i;
				break;
			}
		}

		if (png_start == SIZE_MAX)
		{
			res.body.set_from_string("{\"error\":\"no PNG signature found in field data\"}");
			res.status_code = 422;
			res.content_type = "application/json";
			return;
		}

		// Find IEND chunk
		size_t cur = png_start + 8;
		while (true)
		{
			if (cur + 12 > obj_size)
			{
				res.body.set_from_string("{\"error\":\"truncated PNG before IEND\"}");
				res.status_code = 422;
				res.content_type = "application/json";
				return;
			}

			uint32_t chunk_len = be32(obj + cur);

			// Prevent integer overflow when computing next chunk position
			if (chunk_len > obj_size - cur - 12)
			{
				res.body.set_from_string("{\"error\":\"invalid PNG chunk length\"}");
				res.status_code = 422;
				res.content_type = "application/json";
				return;
			}

			const uint8_t* type = obj + cur + 4;
			size_t next = cur + 4 + 4 + chunk_len + 4;

			if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D')
			{
				size_t png_size = next - png_start;
				res.body.set(obj + png_start, png_size);
				res.status_code = WebResponseCode::OK;
				res.content_type = "image/png";
				return;
			}

			cur = next;
		}
	}

} // namespace robotick
