// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Sync.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/services/WebServer.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringView.h"
#include "robotick/framework/time/Clock.h"
#include "robotick/framework/utility/Algorithm.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#if defined(ROBOTICK_PLATFORM_ESP32S3)
#include <esp_random.h>
#endif

namespace robotick
{
	nlohmann::ordered_json build_workloads_buffer_layout_json(const Engine& engine, const char* session_id_override);

	namespace
	{
		static void set_json_response(WebResponse& res, const int status_code, const nlohmann::ordered_json& payload)
		{
			res.set_status_code(status_code);
			res.set_content_type("application/json");
			const auto body = payload.dump();
			res.set_body_string(body.c_str());
		}

		static bool json_value_to_text(const nlohmann::json& value, FixedString256& out)
		{
			if (value.is_string())
			{
				out = value.get_ref<const nlohmann::json::string_t&>().c_str();
				return true;
			}
			if (value.is_boolean())
			{
				out = value.get<bool>() ? "true" : "false";
				return true;
			}
			if (value.is_number())
			{
				out = value.dump().c_str();
				return true;
			}
			return false;
		}

		static bool is_supported_writable_type(const TypeDescriptor* type_desc)
		{
			if (!type_desc)
			{
				return false;
			}

			if (type_desc->get_enum_desc() != nullptr)
			{
				return true;
			}

			if (type_desc->mime_type == "text/plain")
			{
				return true;
			}

			const StringView& name = type_desc->name;
			return name == "bool" || name == "int" || name == "float" || name == "double" || name == "uint16_t" || name == "uint32_t";
		}
	} // namespace

	struct TelemetryServer::Impl
	{
		static constexpr size_t kMaxWritePayloadBytes = 256;

		struct WritableInputField
		{
			uint16_t handle = 0;
			FixedString256 path;
			const TypeDescriptor* type_desc = nullptr;
			void* target_ptr = nullptr;
			uint16_t value_size = 0;
		};

		struct PendingInputWrite
		{
			bool pending = false;
			uint64_t seq = 0;
			uint16_t payload_size = 0;
			alignas(max_align_t) uint8_t payload[kMaxWritePayloadBytes] = {};
		};

		WebServer web_server;
		const Engine* engine = nullptr;
		FixedString64 session_id;
		HeapVector<WritableInputField> writable_input_fields;
		HeapVector<PendingInputWrite> pending_input_writes;
		Mutex pending_input_writes_mutex;
		uint64_t write_seq_counter = 0;
		bool is_setup = false;

		void rebuild_writable_input_registry();
		int find_writable_input_index_by_handle(uint16_t handle) const;
		int find_writable_input_index_by_path(const char* path) const;
		void handle_get_workloads_buffer_layout(const WebRequest& req, WebResponse& res);
		void handle_get_workloads_buffer_raw(const WebRequest& req, WebResponse& res);
		void handle_set_workload_input_field_data(const WebRequest& req, WebResponse& res);
	};

	TelemetryServer::TelemetryServer()
		: impl(new Impl())
	{
	}

	TelemetryServer::~TelemetryServer()
	{
		stop();
		delete impl;
		impl = nullptr;
	}

	const char* TelemetryServer::get_session_id() const
	{
		return impl ? impl->session_id.c_str() : "";
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

	void TelemetryServer::setup(const Engine& engine_in)
	{
		if (impl)
		{
			impl->web_server.stop();
			delete impl;
			impl = nullptr;
		}
		impl = new Impl();
		impl->engine = &engine_in;
		build_session_id(engine_in.get_model_name(), impl->session_id);
		impl->rebuild_writable_input_registry();
		impl->is_setup = true;
	}

	void TelemetryServer::start(const Engine& engine_in, const uint16_t telemetry_port)
	{
		if (!impl || !impl->is_setup)
		{
			setup(engine_in);
		}
		impl->engine = &engine_in;

		// WebServer owns the socket lifetime; the handler lambda only decides whether a request belongs to the telemetry API.
		impl->web_server.start("Telemetry",
			telemetry_port,
			nullptr,
			[this](const WebRequest& req, WebResponse& res)
			{
				if (!impl || !impl->engine)
				{
					nlohmann::ordered_json body;
					body["error"] = "engine_not_available";
					set_json_response(res, WebResponseCode::ServiceUnavailable, body);
					return true;
				}

				if (req.method.equals("GET"))
				{
					if (req.uri.equals("/api/telemetry/health"))
					{
						res.set_status_code(WebResponseCode::OK);
						return true;
					}
					if (req.uri.equals("/api/telemetry/workloads_buffer/layout"))
					{
						impl->handle_get_workloads_buffer_layout(req, res);
						return true;
					}
					if (req.uri.equals("/api/telemetry/workloads_buffer/raw"))
					{
						impl->handle_get_workloads_buffer_raw(req, res);
						return true;
					}
				}
				else if (req.method.equals("POST"))
				{
					if (req.uri.equals("/api/telemetry/set_workload_input_field_data"))
					{
						impl->handle_set_workload_input_field_data(req, res);
						return true;
					}
				}
				return false;
			});
	}

	void TelemetryServer::stop()
	{
		if (!impl)
		{
			return;
		}
		impl->web_server.stop();
		impl->engine = nullptr;
	}

	void TelemetryServer::apply_pending_input_writes()
	{
		if (!impl || !impl->engine)
		{
			return;
		}

		LockGuard lock(impl->pending_input_writes_mutex);
		const size_t writable_count = impl->writable_input_fields.size();
		const size_t pending_count = impl->pending_input_writes.size();
		const size_t count = min(writable_count, pending_count);
		for (size_t i = 0; i < count; ++i)
		{
			Impl::PendingInputWrite& pending = impl->pending_input_writes[i];
			if (!pending.pending)
			{
				continue;
			}

			Impl::WritableInputField& writable = impl->writable_input_fields[i];
			if (!writable.target_ptr || !writable.type_desc || pending.payload_size != writable.value_size)
			{
				pending.pending = false;
				continue;
			}

			::memcpy(writable.target_ptr, pending.payload, writable.value_size);
			pending.pending = false;
		}
	}

	int TelemetryServer::Impl::find_writable_input_index_by_handle(const uint16_t handle) const
	{
		if (handle == 0)
		{
			return -1;
		}
		for (size_t i = 0; i < writable_input_fields.size(); ++i)
		{
			if (writable_input_fields[i].handle == handle)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	int TelemetryServer::Impl::find_writable_input_index_by_path(const char* path) const
	{
		if (!path || path[0] == '\0')
		{
			return -1;
		}
		for (size_t i = 0; i < writable_input_fields.size(); ++i)
		{
			if (writable_input_fields[i].path == path)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	void TelemetryServer::Impl::rebuild_writable_input_registry()
	{
		write_seq_counter = 0;

		if (!engine)
		{
			return;
		}

		WorkloadsBuffer& workloads_buffer = engine->get_workloads_buffer();
		const auto& instances = engine->get_all_instance_info();

		size_t writable_count = 0;
		for (const WorkloadInstanceInfo& workload_instance_info : instances)
		{
			if (!workload_instance_info.seed || !workload_instance_info.type)
			{
				continue;
			}

			const WorkloadDescriptor* desc = workload_instance_info.type->get_workload_desc();
			if (!desc || !desc->inputs_desc)
			{
				continue;
			}

			const StructDescriptor* inputs_struct = desc->inputs_desc->get_struct_desc();
			if (!inputs_struct)
			{
				continue;
			}

			for (const FieldDescriptor& field_desc : inputs_struct->fields)
			{
				if (field_desc.element_count != 1)
				{
					continue;
				}

				const TypeDescriptor* field_type = field_desc.find_type_descriptor();
				if (!is_supported_writable_type(field_type))
				{
					continue;
				}
				if (!field_type || field_type->size == 0 || field_type->size > kMaxWritePayloadBytes)
				{
					continue;
				}

				void* field_ptr = field_desc.get_data_ptr(workloads_buffer, workload_instance_info, *desc->inputs_desc, desc->inputs_offset);
				if (!workloads_buffer.contains_object_used_space(field_ptr, field_type->size))
				{
					continue;
				}

				++writable_count;
			}
		}

		if (writable_count == 0)
		{
			return;
		}

		if (writable_count > 0xFFFF)
		{
			ROBOTICK_WARNING(
				"Telemetry writable input registry exceeded handle range (%zu > 65535). Truncating to 65535.",
				writable_count);
			writable_count = 0xFFFF;
		}

		writable_input_fields.initialize(writable_count);
		pending_input_writes.initialize(writable_count);

		size_t write_index = 0;
		for (const WorkloadInstanceInfo& workload_instance_info : instances)
		{
			if (write_index >= writable_count)
			{
				break;
			}
			if (!workload_instance_info.seed || !workload_instance_info.type)
			{
				continue;
			}

			const WorkloadDescriptor* desc = workload_instance_info.type->get_workload_desc();
			if (!desc || !desc->inputs_desc)
			{
				continue;
			}

			const StructDescriptor* inputs_struct = desc->inputs_desc->get_struct_desc();
			if (!inputs_struct)
			{
				continue;
			}

			for (const FieldDescriptor& field_desc : inputs_struct->fields)
			{
				if (write_index >= writable_count)
				{
					break;
				}
				if (field_desc.element_count != 1)
				{
					continue;
				}

				const TypeDescriptor* field_type = field_desc.find_type_descriptor();
				if (!is_supported_writable_type(field_type))
				{
					continue;
				}
				if (!field_type || field_type->size == 0 || field_type->size > kMaxWritePayloadBytes)
				{
					continue;
				}

				void* field_ptr = field_desc.get_data_ptr(workloads_buffer, workload_instance_info, *desc->inputs_desc, desc->inputs_offset);
				if (!workloads_buffer.contains_object_used_space(field_ptr, field_type->size))
				{
					continue;
				}

				WritableInputField& writable = writable_input_fields[write_index];
				writable.handle = static_cast<uint16_t>(write_index + 1);
				writable.path.format("%s.inputs.%s", workload_instance_info.seed->unique_name.c_str(), field_desc.name.c_str());
				writable.type_desc = field_type;
				writable.target_ptr = field_ptr;
				writable.value_size = static_cast<uint16_t>(field_type->size);
				pending_input_writes[write_index] = PendingInputWrite{};
				++write_index;
			}
		}
	}

	void TelemetryServer::Impl::handle_set_workload_input_field_data(const WebRequest& req, WebResponse& res)
	{
		nlohmann::ordered_json response_json;
		if (!engine)
		{
			response_json["error"] = "engine_not_available";
			set_json_response(res, WebResponseCode::ServiceUnavailable, response_json);
			return;
		}

		const uint8_t* body_begin = req.body.data();
		const uint8_t* body_end = body_begin + req.body.size();
		const nlohmann::json payload = nlohmann::json::parse(body_begin, body_end, nullptr, /*allow_exceptions*/ false);
		if (payload.is_discarded() || !payload.is_object())
		{
			response_json["error"] = "invalid_json";
			set_json_response(res, WebResponseCode::BadRequest, response_json);
			return;
		}

		const nlohmann::json session_node = payload.contains("engine_session_id") ? payload["engine_session_id"] : nlohmann::json();
		if (!session_node.is_string())
		{
			response_json["error"] = "missing_engine_session_id";
			set_json_response(res, WebResponseCode::BadRequest, response_json);
			return;
		}

		const auto& request_session = session_node.get_ref<const nlohmann::json::string_t&>();
		if (!StringView(session_id.c_str()).equals(request_session.c_str()))
		{
			response_json["error"] = "session_mismatch";
			response_json["engine_session_id"] = session_id.c_str();
			set_json_response(res, WebResponseCode::PreconditionFailed, response_json);
			return;
		}

		int writable_index = -1;
		uint16_t writable_handle = 0;
		FixedString256 requested_path;

		if (payload.contains("field_handle") && payload["field_handle"].is_number_integer())
		{
			const long long handle_value = payload["field_handle"].get<long long>();
			if (handle_value <= 0 || handle_value > 0xFFFF)
			{
				response_json["error"] = "invalid_field_handle";
				set_json_response(res, WebResponseCode::BadRequest, response_json);
				return;
			}
			writable_handle = static_cast<uint16_t>(handle_value);
			writable_index = find_writable_input_index_by_handle(writable_handle);
		}

		if (payload.contains("field_path") && payload["field_path"].is_string())
		{
			requested_path = payload["field_path"].get_ref<const nlohmann::json::string_t&>().c_str();

			const int path_index = find_writable_input_index_by_path(requested_path.c_str());
			if (path_index >= 0)
			{
				if (writable_index >= 0 && static_cast<size_t>(writable_index) != static_cast<size_t>(path_index))
				{
					response_json["error"] = "field_handle_path_mismatch";
					set_json_response(res, WebResponseCode::BadRequest, response_json);
					return;
				}
				writable_index = path_index;
				writable_handle = writable_input_fields[static_cast<size_t>(writable_index)].handle;
			}
		}

		if (writable_index < 0)
		{
			response_json["error"] = "writable_input_not_found";
			set_json_response(res, WebResponseCode::NotFound, response_json);
			return;
		}

		if (!payload.contains("value"))
		{
			response_json["error"] = "missing_value";
			set_json_response(res, WebResponseCode::BadRequest, response_json);
			return;
		}

		WritableInputField& writable = writable_input_fields[static_cast<size_t>(writable_index)];
		if (!writable.type_desc || writable.value_size == 0 || writable.value_size > kMaxWritePayloadBytes)
		{
			response_json["error"] = "unsupported_target_field";
			set_json_response(res, WebResponseCode::BadRequest, response_json);
			return;
		}

		FixedString256 value_text;
		if (!json_value_to_text(payload["value"], value_text))
		{
			response_json["error"] = "unsupported_value_type";
			set_json_response(res, WebResponseCode::BadRequest, response_json);
			return;
		}

		PendingInputWrite staged;
		staged.pending = true;
		staged.payload_size = writable.value_size;
		::memset(staged.payload, 0, sizeof(staged.payload));
		if (!writable.type_desc->from_string(value_text.c_str(), staged.payload))
		{
			response_json["error"] = "value_parse_failed";
			set_json_response(res, WebResponseCode::BadRequest, response_json);
			return;
		}

		if (payload.contains("seq") && payload["seq"].is_number_integer())
		{
			const long long seq_value = payload["seq"].get<long long>();
			staged.seq = seq_value > 0 ? static_cast<uint64_t>(seq_value) : 0;
		}

		{
			LockGuard lock(pending_input_writes_mutex);
			if (staged.seq == 0)
			{
				write_seq_counter += 1;
				staged.seq = write_seq_counter;
			}
			pending_input_writes[static_cast<size_t>(writable_index)] = staged;
		}

		response_json["status"] = "accepted";
		response_json["field_handle"] = writable_handle;
		response_json["field_path"] = writable.path.c_str();
		response_json["seq"] = staged.seq;
		set_json_response(res, WebResponseCode::OK, response_json);
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

		const EnumDescriptor* enum_desc = type_desc->get_enum_desc();
		if (enum_desc)
		{
			type_json["enum_values"] = nlohmann::ordered_json::array();
			for (const EnumValue& enum_value : enum_desc->values)
			{
				nlohmann::ordered_json enum_json;
				enum_json["name"] = enum_value.name.c_str();
				enum_json["value"] = enum_value.value;
				type_json["enum_values"].push_back(enum_json);
			}
			type_json["enum_underlying_size"] = static_cast<int>(enum_desc->underlying_size);
			type_json["enum_is_signed"] = enum_desc->is_signed;
			type_json["enum_is_flags"] = enum_desc->is_flags;
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

	static nlohmann::ordered_json build_layout_json(const Engine& engine)
	{
		WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer();
		const auto& instances = engine.get_all_instance_info();

		nlohmann::ordered_json layout_json;
		layout_json["workloads_buffer_size_used"] = workloads_buffer.get_size_used();
		layout_json["process_memory_used"] = get_process_memory_used();
		layout_json["workloads"] = nlohmann::ordered_json::array();
		layout_json["types"] = nlohmann::ordered_json::array();

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

			workload_json["stats_offset_within_container"] =
				static_cast<int>((uint8_t*)workload_instance_info.workload_stats - workloads_buffer.raw_ptr());

			emit_type_info(layout_json, workloads_buffer, workload_instance_info.workload_stats, workload_stats_type);

			layout_json["workloads"].push_back(workload_json);
		}

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

		return layout_json;
	}

	void TelemetryServer::Impl::handle_get_workloads_buffer_layout(const WebRequest& /*req*/, WebResponse& res)
	{
		nlohmann::ordered_json layout_json = build_workloads_buffer_layout_json(*engine, session_id.c_str());
		layout_json["writable_inputs"] = nlohmann::ordered_json::array();

		const size_t writable_count = writable_input_fields.size();
		for (size_t i = 0; i < writable_count; ++i)
		{
			const WritableInputField& writable = writable_input_fields[i];
			nlohmann::ordered_json writable_json;
			writable_json["field_handle"] = writable.handle;
			writable_json["field_path"] = writable.path.c_str();
			writable_json["type"] = writable.type_desc ? writable.type_desc->name.c_str() : "unknown";
			writable_json["size"] = writable.value_size;
			layout_json["writable_inputs"].push_back(writable_json);
		}

		res.set_status_code(WebResponseCode::OK);
		res.set_content_type("application/json");

		auto out_str = layout_json.dump(2); // Pretty print
		res.set_body_string(out_str.c_str());
	}

	void TelemetryServer::Impl::handle_get_workloads_buffer_raw(const WebRequest& /*req*/, WebResponse& res)
	{
		const WorkloadsBuffer& workloads_buffer = engine->get_workloads_buffer();

		res.set_status_code(WebResponseCode::OK);
		res.set_content_type("application/octet-stream");

		// Attach the session ID as a custom response header
		FixedString256 session_header;
		session_header.format("X-Robotick-Session-Id:%s", session_id.c_str());
		res.add_header(session_header.c_str());

		FixedString256 frame_seq_header;
		frame_seq_header.format("X-Robotick-Frame-Seq:%u", static_cast<unsigned int>(workloads_buffer.get_telemetry_frame_seq()));
		res.add_header(frame_seq_header.c_str());

		res.set_body(workloads_buffer.raw_ptr(), workloads_buffer.get_size_used());
	}

	nlohmann::ordered_json build_workloads_buffer_layout_json(const Engine& engine, const char* session_id_override)
	{
		nlohmann::ordered_json layout_json = build_layout_json(engine);
		layout_json["engine_session_id"] = session_id_override ? session_id_override : "";
		return layout_json;
	}

} // namespace robotick
