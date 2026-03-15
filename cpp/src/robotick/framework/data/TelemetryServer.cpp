// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Sync.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/json/Json.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/services/WebServer.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringView.h"
#include "robotick/framework/time/Clock.h"
#include "robotick/framework/utility/Algorithm.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <unistd.h>

#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
#include <curl/curl.h>
#endif

#if defined(ROBOTICK_PLATFORM_ESP32S3)
#include <esp_random.h>
#endif

namespace robotick
{
	namespace
	{
		template <typename BuildFn> static void set_json_response(WebResponse& res, const int status_code, BuildFn&& build_json)
		{
			json::StringSink sink;
			json::Writer<json::StringSink> writer(sink);
			build_json(writer);
			writer.flush();
			res.set_status_code(status_code);
			res.set_content_type("application/json");
			res.set_body(sink.c_str(), sink.size());
		}

		static void set_error_response(WebResponse& res, const int status_code, const char* error_text)
		{
			set_json_response(res,
				status_code,
				[&](auto& writer)
				{
					writer.start_object();
					writer.key("error");
					writer.string(error_text);
					writer.end_object();
				});
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

		static const StructDescriptor* try_get_struct_descriptor(const TypeDescriptor* type_desc, const void* instance_ptr)
		{
			if (!type_desc)
			{
				return nullptr;
			}
			if (const StructDescriptor* struct_desc = type_desc->get_struct_desc())
			{
				return struct_desc;
			}
			if (const DynamicStructDescriptor* dynamic_struct_desc = type_desc->get_dynamic_struct_desc())
			{
				return dynamic_struct_desc->get_struct_descriptor(instance_ptr);
			}
			return nullptr;
		}

		static bool has_incoming_connection_overlap(
			const HeapVector<DataConnectionInfo>& connections, const void* target_ptr, const size_t target_size)
		{
			if (!target_ptr || target_size == 0)
			{
				return false;
			}

			const uintptr_t target_begin = reinterpret_cast<uintptr_t>(target_ptr);
			const uintptr_t target_end = target_begin + target_size;

			for (const DataConnectionInfo& connection : connections)
			{
				if (!connection.dest_ptr || connection.size == 0)
				{
					continue;
				}

				const uintptr_t connection_begin = reinterpret_cast<uintptr_t>(connection.dest_ptr);
				const uintptr_t connection_end = connection_begin + connection.size;
				if (target_begin < connection_end && connection_begin < target_end)
				{
					return true;
				}
			}
			return false;
		}

		static bool uri_equals(const char* lhs, const char* rhs)
		{
			return lhs && rhs && StringView(lhs).equals(rhs);
		}

		static bool starts_with(const char* text, const char* prefix)
		{
			return text && prefix && ::strncmp(text, prefix, ::strlen(prefix)) == 0;
		}

		static const char* skip_prefix(const char* text, const char* prefix)
		{
			return starts_with(text, prefix) ? text + ::strlen(prefix) : nullptr;
		}

		static bool try_parse_model_routed_uri(const char* uri, FixedString64& model_id, FixedString128& suffix_uri)
		{
			constexpr const char* prefix = "/api/telemetry-gateway/";
			const char* rest = skip_prefix(uri, prefix);
			if (!rest || *rest == '\0')
			{
				return false;
			}

			if (StringView(rest).equals("models"))
			{
				return false;
			}

			const char* slash = ::strchr(rest, '/');
			if (!slash || slash == rest)
			{
				return false;
			}

			model_id.assign(rest, static_cast<size_t>(slash - rest));
			suffix_uri = slash;
			return !model_id.empty() && !suffix_uri.empty();
		}

#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		struct ForwardHttpResult
		{
			int status_code = WebResponseCode::ServiceUnavailable;
			bool transport_ok = false;
			FixedString128 content_type;
			FixedString128 session_header;
			FixedString128 frame_seq_header;
			HeapVector<uint8_t> body;
		};

		struct ForwardHttpBuffer
		{
			FILE* file = nullptr;
			ForwardHttpResult* result = nullptr;
		};

		static size_t curl_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
		{
			ForwardHttpBuffer* buffer = static_cast<ForwardHttpBuffer*>(userdata);
			if (!buffer || !buffer->file)
			{
				return 0;
			}
			return ::fwrite(ptr, size, nmemb, buffer->file) * size;
		}

		static void try_store_header_value(const char* line, const char* header_name, FixedString128& target)
		{
			const size_t header_len = ::strlen(header_name);
			if (::strncasecmp(line, header_name, header_len) != 0)
			{
				return;
			}
			const char* value = line + header_len;
			while (*value == ' ' || *value == '\t')
			{
				++value;
			}
			const char* end = value + ::strlen(value);
			while (end > value && (end[-1] == '\r' || end[-1] == '\n'))
			{
				--end;
			}
			target.assign(value, static_cast<size_t>(end - value));
		}

		static size_t curl_header_callback(char* buffer, size_t size, size_t nitems, void* userdata)
		{
			ForwardHttpResult* result = static_cast<ForwardHttpResult*>(userdata);
			const size_t total = size * nitems;
			if (!result || total == 0)
			{
				return total;
			}

			FixedString256 line;
			line.assign(buffer, total);
			try_store_header_value(line.c_str(), "Content-Type:", result->content_type);
			try_store_header_value(line.c_str(), "X-Robotick-Session-Id:", result->session_header);
			try_store_header_value(line.c_str(), "X-Robotick-Frame-Seq:", result->frame_seq_header);
			return total;
		}

		static bool perform_forwarded_http_request(const char* method, const char* url, const WebRequest& request, ForwardHttpResult& out_result)
		{
			CURL* curl = curl_easy_init();
			if (!curl)
			{
				return false;
			}

			FILE* tmp = ::tmpfile();
			if (!tmp)
			{
				curl_easy_cleanup(curl);
				return false;
			}

			ForwardHttpBuffer buffer;
			buffer.file = tmp;
			buffer.result = &out_result;

			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_callback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &out_result);

			if (StringView(method).equals("POST"))
			{
				curl_easy_setopt(curl, CURLOPT_POST, 1L);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.size() > 0 ? reinterpret_cast<const char*>(request.body.data()) : "");
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
			}

			const CURLcode curl_code = curl_easy_perform(curl);
			long response_code = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
			curl_easy_cleanup(curl);

			if (curl_code != CURLE_OK)
			{
				::fclose(tmp);
				return false;
			}

			out_result.transport_ok = true;
			out_result.status_code = response_code > 0 ? static_cast<int>(response_code) : WebResponseCode::OK;

			if (::fseek(tmp, 0, SEEK_END) != 0)
			{
				::fclose(tmp);
				return true;
			}
			const long file_size = ::ftell(tmp);
			if (file_size > 0)
			{
				::rewind(tmp);
				out_result.body.initialize(static_cast<size_t>(file_size));
				const size_t bytes_read = ::fread(out_result.body.data(), 1, static_cast<size_t>(file_size), tmp);
				if (bytes_read != static_cast<size_t>(file_size))
				{
					out_result.transport_ok = false;
				}
			}
			::fclose(tmp);
			return true;
		}
#endif
	} // namespace

	struct TelemetryServer::Impl
	{
		static constexpr size_t kMaxWritePayloadBytes = 256;

		struct WritableInputField
		{
			uint16_t handle = 0;
			FixedString512 path;
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

		struct TelemetryPeerRoute
		{
			FixedString64 model_id;
			FixedString128 host;
			uint16_t telemetry_port = 0;
			bool is_gateway = false;
		};

		WebServer web_server;
		const Engine* engine = nullptr;
		FixedString64 session_id;
		HeapVector<WritableInputField> writable_input_fields;
		HeapVector<PendingInputWrite> pending_input_writes;
		Mutex pending_input_writes_mutex;
		uint64_t write_seq_counter = 0;
		bool is_setup = false;
		bool is_gateway = false;
		HeapVector<TelemetryPeerRoute> telemetry_peers;

		void rebuild_writable_input_registry();
		int find_writable_input_index_by_handle(uint16_t handle) const;
		int find_writable_input_index_by_path(const char* path) const;
		int find_telemetry_peer_index_by_model_id(const char* model_id) const;
		void rebuild_telemetry_peer_registry();
		bool handle_local_telemetry_request(const WebRequest& req, WebResponse& res, const char* effective_uri);
		void handle_get_gateway_models(WebResponse& res);
		void handle_forwarded_telemetry_request(const WebRequest& req, WebResponse& res, const TelemetryPeerRoute& peer, const char* suffix_uri);
		void handle_get_workloads_buffer_layout(const WebRequest& req, WebResponse& res);
		void handle_get_workloads_buffer_raw(const WebRequest& req, WebResponse& res);
		void handle_set_workload_input_fields_data(const WebRequest& req, WebResponse& res);
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
		impl->is_gateway = engine_in.get_model().get_telemetry_is_gateway();
		build_session_id(engine_in.get_model_name(), impl->session_id);
		impl->rebuild_writable_input_registry();
		impl->rebuild_telemetry_peer_registry();
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
					set_error_response(res, WebResponseCode::ServiceUnavailable, "engine_not_available");
					return true;
				}

				if (impl->is_gateway && req.method.equals("GET") && req.uri.equals("/api/telemetry-gateway/models"))
				{
					impl->handle_get_gateway_models(res);
					return true;
				}

				if (impl->is_gateway)
				{
					FixedString64 routed_model_id;
					FixedString128 routed_suffix_uri;
					if (try_parse_model_routed_uri(req.uri.c_str(), routed_model_id, routed_suffix_uri))
					{
						if (routed_model_id == impl->engine->get_model_name())
						{
							return impl->handle_local_telemetry_request(req, res, routed_suffix_uri.c_str());
						}

						const int peer_index = impl->find_telemetry_peer_index_by_model_id(routed_model_id.c_str());
						if (peer_index >= 0)
						{
							impl->handle_forwarded_telemetry_request(
								req, res, impl->telemetry_peers[static_cast<size_t>(peer_index)], routed_suffix_uri.c_str());
							return true;
						}

						set_json_response(res,
							WebResponseCode::NotFound,
							[&](auto& writer)
							{
								writer.start_object();
								writer.key("error");
								writer.string("telemetry_peer_not_found");
								writer.key("model_id");
								writer.string(routed_model_id.c_str());
								writer.end_object();
							});
						return true;
					}
				}

				return impl->handle_local_telemetry_request(req, res, req.uri.c_str());
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

	void TelemetryServer::update_peer_route(const char* model_id, const char* host, const uint16_t telemetry_port, const bool is_gateway)
	{
		if (!impl || !model_id || !host || model_id[0] == '\0' || host[0] == '\0' || telemetry_port == 0)
		{
			return;
		}

		const int peer_index = impl->find_telemetry_peer_index_by_model_id(model_id);
		if (peer_index < 0)
		{
			return;
		}

		Impl::TelemetryPeerRoute& peer = impl->telemetry_peers[static_cast<size_t>(peer_index)];
		peer.host = host;
		peer.telemetry_port = telemetry_port;
		peer.is_gateway = is_gateway;
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

	int TelemetryServer::Impl::find_telemetry_peer_index_by_model_id(const char* model_id) const
	{
		if (!model_id || model_id[0] == '\0')
		{
			return -1;
		}
		for (size_t i = 0; i < telemetry_peers.size(); ++i)
		{
			if (telemetry_peers[i].model_id == model_id)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	void TelemetryServer::Impl::rebuild_telemetry_peer_registry()
	{
		if (!engine)
		{
			return;
		}

		const auto& peers = engine->get_model().get_telemetry_peers();
		if (peers.empty())
		{
			return;
		}

		telemetry_peers.initialize(peers.size());
		for (size_t i = 0; i < peers.size(); ++i)
		{
			const TelemetryPeerSeed* seed = peers[i];
			if (!seed)
			{
				continue;
			}
			TelemetryPeerRoute& route = telemetry_peers[i];
			route.model_id = seed->model_name.c_str();
			route.host = seed->host.c_str();
			route.telemetry_port = seed->telemetry_port;
			route.is_gateway = seed->is_gateway;
		}
	}

	bool TelemetryServer::Impl::handle_local_telemetry_request(const WebRequest& req, WebResponse& res, const char* effective_uri)
	{
		if (req.method.equals("GET"))
		{
			if (uri_equals(effective_uri, "/api/telemetry/health") || uri_equals(effective_uri, "/health"))
			{
				res.set_status_code(WebResponseCode::OK);
				res.set_body(nullptr, 0);
				return true;
			}
			if (uri_equals(effective_uri, "/api/telemetry/workloads_buffer/layout") || uri_equals(effective_uri, "/workloads_buffer/layout"))
			{
				handle_get_workloads_buffer_layout(req, res);
				return true;
			}
			if (uri_equals(effective_uri, "/api/telemetry/workloads_buffer/raw") || uri_equals(effective_uri, "/workloads_buffer/raw"))
			{
				handle_get_workloads_buffer_raw(req, res);
				return true;
			}
		}
		else if (req.method.equals("POST"))
		{
			if (uri_equals(effective_uri, "/api/telemetry/set_workload_input_fields_data") ||
				uri_equals(effective_uri, "/set_workload_input_fields_data"))
			{
				handle_set_workload_input_fields_data(req, res);
				return true;
			}
		}
		return false;
	}

	void TelemetryServer::Impl::handle_get_gateway_models(WebResponse& res)
	{
		set_json_response(res,
			WebResponseCode::OK,
			[&](auto& writer)
			{
				writer.start_object();
				writer.key("gateway_model_id");
				writer.string(engine ? engine->get_model_name() : "");
				writer.key("models");
				writer.start_array();

				FixedString128 local_path;
				local_path.format("/api/telemetry-gateway/%s", engine ? engine->get_model_name() : "");

				writer.start_object();
				writer.key("model_id");
				writer.string(engine ? engine->get_model_name() : "");
				writer.key("is_local");
				writer.boolean(true);
				writer.key("is_gateway");
				writer.boolean(true);
				writer.key("telemetry_path");
				writer.string(local_path);
				writer.end_object();

				for (const TelemetryPeerRoute& peer : telemetry_peers)
				{
					FixedString128 path;
					path.format("/api/telemetry-gateway/%s", peer.model_id.c_str());

					writer.start_object();
					writer.key("model_id");
					writer.string(peer.model_id);
					writer.key("is_local");
					writer.boolean(false);
					writer.key("is_gateway");
					writer.boolean(peer.is_gateway);
					writer.key("telemetry_path");
					writer.string(path);
					writer.end_object();
				}

				writer.end_array();
				writer.end_object();
			});
	}

	void TelemetryServer::Impl::handle_forwarded_telemetry_request(
		const WebRequest& req, WebResponse& res, const TelemetryPeerRoute& peer, const char* suffix_uri)
	{
#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		FixedString256 url;
		url.format("http://%s:%u/api/telemetry%s", peer.host.c_str(), static_cast<unsigned int>(peer.telemetry_port), suffix_uri);

		ForwardHttpResult forwarded;
		if (!perform_forwarded_http_request(req.method.c_str(), url.c_str(), req, forwarded) || !forwarded.transport_ok)
		{
			set_json_response(res,
				WebResponseCode::ServiceUnavailable,
				[&](auto& writer)
				{
					writer.start_object();
					writer.key("error");
					writer.string("telemetry_forward_failed");
					writer.key("model_id");
					writer.string(peer.model_id);
					writer.key("target_url");
					writer.string(url);
					writer.end_object();
				});
			return;
		}

		res.set_status_code(forwarded.status_code);
		if (!forwarded.content_type.empty())
		{
			res.set_content_type(forwarded.content_type.c_str());
		}
		if (!forwarded.session_header.empty())
		{
			FixedString256 header;
			header.format("X-Robotick-Session-Id:%s", forwarded.session_header.c_str());
			res.add_header(header.c_str());
		}
		if (!forwarded.frame_seq_header.empty())
		{
			FixedString256 header;
			header.format("X-Robotick-Frame-Seq:%s", forwarded.frame_seq_header.c_str());
			res.add_header(header.c_str());
		}
		res.set_body(forwarded.body.size() > 0 ? reinterpret_cast<const void*>(forwarded.body.data()) : nullptr, forwarded.body.size());
#else
		(void)req;
		(void)peer;
		(void)suffix_uri;
		set_error_response(res, WebResponseCode::NotImplemented, "telemetry_forwarding_not_supported_on_this_platform");
#endif
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
		const auto& incoming_connections = engine->get_all_data_connections();

		const auto for_each_writable_input_leaf = [&](auto&& on_leaf)
		{
			for (const WorkloadInstanceInfo& workload_instance_info : instances)
			{
				if (!workload_instance_info.seed || !workload_instance_info.type)
				{
					continue;
				}

				const WorkloadDescriptor* desc = workload_instance_info.type->get_workload_desc();
				if (!desc || !desc->inputs_desc || desc->inputs_offset == OFFSET_UNBOUND)
				{
					continue;
				}

				const TypeDescriptor* inputs_type = desc->inputs_desc;
				const uint8_t* workload_ptr = workload_instance_info.get_ptr(workloads_buffer);
				if (!workload_ptr)
				{
					continue;
				}

				void* inputs_ptr = const_cast<uint8_t*>(workload_ptr) + desc->inputs_offset;
				if (!workloads_buffer.contains_object_used_space(inputs_ptr, inputs_type->size))
				{
					continue;
				}

				FixedString512 root_path;
				root_path.format("%s.inputs", workload_instance_info.seed->unique_name.c_str());

				const auto walk_struct = [&](const auto& self,
											 const TypeDescriptor* struct_type,
											 void* struct_ptr,
											 const FixedString512& path_prefix,
											 auto&& on_leaf_ref) -> void
				{
					const StructDescriptor* struct_desc = try_get_struct_descriptor(struct_type, struct_ptr);
					if (!struct_desc)
					{
						return;
					}

					for (const FieldDescriptor& field_desc : struct_desc->fields)
					{
						if (field_desc.element_count != 1)
						{
							continue;
						}

						const TypeDescriptor* field_type = field_desc.find_type_descriptor();
						if (!field_type)
						{
							continue;
						}

						void* field_ptr = field_desc.get_data_ptr(struct_ptr);
						if (!field_ptr || !workloads_buffer.contains_object_used_space(field_ptr, field_type->size))
						{
							continue;
						}

						FixedString512 field_path;
						field_path.format("%s.%s", path_prefix.c_str(), field_desc.name.c_str());

						if (try_get_struct_descriptor(field_type, field_ptr))
						{
							self(self, field_type, field_ptr, field_path, on_leaf_ref);
							continue;
						}

						if (!is_supported_writable_type(field_type))
						{
							continue;
						}
						if (field_type->size == 0 || field_type->size > kMaxWritePayloadBytes)
						{
							continue;
						}
						if (has_incoming_connection_overlap(incoming_connections, field_ptr, field_type->size))
						{
							continue;
						}

						on_leaf_ref(field_path, field_type, field_ptr);
					}
				};

				walk_struct(walk_struct, inputs_type, inputs_ptr, root_path, on_leaf);
			}
		};

		size_t writable_count = 0;
		for_each_writable_input_leaf(
			[&](const FixedString512&, const TypeDescriptor*, void*)
			{
				++writable_count;
			});

		if (writable_count == 0)
		{
			return;
		}

		if (writable_count > 0xFFFF)
		{
			ROBOTICK_WARNING("Telemetry writable input registry exceeded handle range (%zu > 65535). Truncating to 65535.", writable_count);
			writable_count = 0xFFFF;
		}

		writable_input_fields.initialize(writable_count);
		pending_input_writes.initialize(writable_count);

		size_t write_index = 0;
		for_each_writable_input_leaf(
			[&](const FixedString512& field_path, const TypeDescriptor* field_type, void* field_ptr)
			{
				if (write_index >= writable_count)
				{
					return;
				}

				WritableInputField& writable = writable_input_fields[write_index];
				writable.handle = static_cast<uint16_t>(write_index + 1);
				writable.path = field_path.c_str();
				writable.type_desc = field_type;
				writable.target_ptr = field_ptr;
				writable.value_size = static_cast<uint16_t>(field_type ? field_type->size : 0);
				pending_input_writes[write_index] = PendingInputWrite{};
				++write_index;
			});
	}

	void TelemetryServer::Impl::handle_set_workload_input_fields_data(const WebRequest& req, WebResponse& res)
	{
		struct ParsedWrite
		{
			size_t writable_index = 0;
			uint16_t writable_handle = 0;
			PendingInputWrite staged;
		};

		if (!engine)
		{
			set_error_response(res, WebResponseCode::ServiceUnavailable, "engine_not_available");
			return;
		}

		json::Document payload_document;
		if (!payload_document.parse(req.body.data(), req.body.size()))
		{
			set_error_response(res, WebResponseCode::BadRequest, "invalid_json");
			return;
		}
		const json::Value payload = payload_document.root();

		const json::Value session_node = payload["engine_session_id"];
		if (!session_node.is_string())
		{
			set_error_response(res, WebResponseCode::BadRequest, "missing_engine_session_id");
			return;
		}

		if (!StringView(session_id.c_str()).equals(session_node.get_c_string()))
		{
			set_json_response(res,
				WebResponseCode::PreconditionFailed,
				[&](auto& writer)
				{
					writer.start_object();
					writer.key("error");
					writer.string("session_mismatch");
					writer.key("engine_session_id");
					writer.string(session_id.c_str());
					writer.end_object();
				});
			return;
		}

		const json::Value writes_node = payload["writes"];
		if (!writes_node.is_array() || writes_node.size() == 0)
		{
			set_error_response(res, WebResponseCode::BadRequest, "missing_writes");
			return;
		}

		HeapVector<ParsedWrite> parsed_writes;
		parsed_writes.initialize(writes_node.size());
		size_t parsed_count = 0;

		bool parse_failed = false;
		int parse_failed_status = WebResponseCode::BadRequest;
		const char* parse_failed_error = nullptr;

		writes_node.for_each_array(
			[&](const json::Value write_payload)
			{
				if (parse_failed)
				{
					return;
				}

				if (!write_payload.is_object())
				{
					parse_failed = true;
					parse_failed_error = "invalid_write_entry";
					return;
				}

				int writable_index = -1;
				uint16_t writable_handle = 0;
				FixedString512 requested_path;

				const json::Value field_handle_node = write_payload["field_handle"];
				if (field_handle_node.is_integer())
				{
					const int64_t handle_value = field_handle_node.get_int64();
					if (handle_value <= 0 || handle_value > 0xFFFF)
					{
						parse_failed = true;
						parse_failed_error = "invalid_field_handle";
						return;
					}
					writable_handle = static_cast<uint16_t>(handle_value);
					writable_index = find_writable_input_index_by_handle(writable_handle);
				}

				const json::Value field_path_node = write_payload["field_path"];
				if (field_path_node.is_string())
				{
					requested_path = field_path_node.get_c_string();
					const int path_index = find_writable_input_index_by_path(requested_path.c_str());
					if (path_index >= 0)
					{
						if (writable_index >= 0 && static_cast<size_t>(writable_index) != static_cast<size_t>(path_index))
						{
							parse_failed = true;
							parse_failed_error = "field_handle_path_mismatch";
							return;
						}
						writable_index = path_index;
						writable_handle = writable_input_fields[static_cast<size_t>(writable_index)].handle;
					}
				}

				if (writable_index < 0)
				{
					parse_failed = true;
					parse_failed_status = WebResponseCode::NotFound;
					parse_failed_error = "writable_input_not_found";
					return;
				}

				if (!write_payload.contains("value"))
				{
					parse_failed = true;
					parse_failed_error = "missing_value";
					return;
				}

				WritableInputField& writable = writable_input_fields[static_cast<size_t>(writable_index)];
				if (!writable.type_desc || writable.value_size == 0 || writable.value_size > kMaxWritePayloadBytes)
				{
					parse_failed = true;
					parse_failed_error = "unsupported_target_field";
					return;
				}

				FixedString256 value_text;
				if (!json::scalar_to_fixed_string(write_payload["value"], value_text))
				{
					parse_failed = true;
					parse_failed_error = "unsupported_value_type";
					return;
				}

				ParsedWrite& parsed = parsed_writes[parsed_count];
				parsed.writable_index = static_cast<size_t>(writable_index);
				parsed.writable_handle = writable_handle;
				parsed.staged.pending = true;
				parsed.staged.payload_size = writable.value_size;
				::memset(parsed.staged.payload, 0, sizeof(parsed.staged.payload));
				if (!writable.type_desc->from_string(value_text.c_str(), parsed.staged.payload))
				{
					parse_failed = true;
					parse_failed_error = "value_parse_failed";
					return;
				}

				const json::Value seq_node = write_payload["seq"];
				if (seq_node.is_integer())
				{
					const int64_t seq_value = seq_node.get_int64();
					parsed.staged.seq = seq_value > 0 ? static_cast<uint64_t>(seq_value) : 0;
				}

				parsed_count += 1;
			});

		if (parse_failed)
		{
			set_error_response(res, parse_failed_status, parse_failed_error);
			return;
		}

		struct WriteResult
		{
			uint16_t field_handle = 0;
			FixedString512 field_path;
			uint64_t seq = 0;
			const char* status = "";
			uint64_t latest_seq = 0;
			bool has_latest_seq = false;
		};

		HeapVector<WriteResult> write_results;
		write_results.initialize(parsed_count);
		size_t accepted_count = 0;
		size_t ignored_stale_count = 0;

		{
			LockGuard lock(pending_input_writes_mutex);
			for (size_t i = 0; i < parsed_count; ++i)
			{
				ParsedWrite& parsed = parsed_writes[i];
				WritableInputField& writable = writable_input_fields[parsed.writable_index];
				if (parsed.staged.seq == 0)
				{
					write_seq_counter += 1;
					parsed.staged.seq = write_seq_counter;
				}

				PendingInputWrite& pending = pending_input_writes[parsed.writable_index];
				WriteResult& write_result = write_results[i];
				write_result.field_handle = parsed.writable_handle;
				write_result.field_path = writable.path.c_str();
				write_result.seq = parsed.staged.seq;

				if (pending.seq != 0 && parsed.staged.seq <= pending.seq)
				{
					write_result.status = "ignored_stale";
					write_result.latest_seq = pending.seq;
					write_result.has_latest_seq = true;
					ignored_stale_count += 1;
				}
				else
				{
					pending = parsed.staged;
					write_result.status = "accepted";
					accepted_count += 1;
				}
			}
		}

		set_json_response(res,
			WebResponseCode::OK,
			[&](auto& writer)
			{
				writer.start_object();
				writer.key("status");
				writer.string("processed");
				writer.key("writes");
				writer.start_array();
				for (size_t i = 0; i < parsed_count; ++i)
				{
					const WriteResult& write_result = write_results[i];
					writer.start_object();
					writer.key("field_handle");
					writer.uint64(write_result.field_handle);
					writer.key("field_path");
					writer.string(write_result.field_path);
					writer.key("seq");
					writer.uint64(write_result.seq);
					writer.key("status");
					writer.string(write_result.status);
					if (write_result.has_latest_seq)
					{
						writer.key("latest_seq");
						writer.uint64(write_result.latest_seq);
					}
					writer.end_object();
				}
				writer.end_array();
				writer.key("accepted_count");
				writer.uint64(accepted_count);
				writer.key("ignored_stale_count");
				writer.uint64(ignored_stale_count);
				writer.end_object();
			});
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

	struct EmittedTypeNames
	{
		HeapVector<FixedString256> names;
		size_t count = 0;

		void initialize(const size_t capacity)
		{
			names.initialize(capacity);
			count = 0;
		}

		bool contains(const char* name) const
		{
			for (size_t i = 0; i < count; ++i)
			{
				if (names[i] == name)
				{
					return true;
				}
			}
			return false;
		}

		void add(const char* name)
		{
			ROBOTICK_ASSERT_MSG(count < names.size(), "Telemetry layout emitted type capacity exceeded");
			names[count++] = name ? name : "";
		}
	};

	static size_t get_process_memory_used();

	template <typename Writer> static void write_json_struct_ref(Writer& writer, const char* type_name, const size_t offset_within_container)
	{
		writer.start_object();
		writer.key("type");
		writer.string(type_name ? type_name : "null");
		writer.key("offset_within_container");
		writer.int32(static_cast<int>(offset_within_container));
		writer.end_object();
	}

	template <typename Writer>
	static void emit_type_info_stream(Writer& writer,
		const WorkloadsBuffer& workloads_buffer,
		void* data_ptr,
		const TypeDescriptor* type_desc,
		EmittedTypeNames& emitted_type_names)
	{
		if (!type_desc)
		{
			return;
		}

		const FixedString256 type_name = get_type_name(*type_desc, data_ptr);
		if (emitted_type_names.contains(type_name.c_str()))
		{
			return;
		}
		emitted_type_names.add(type_name.c_str());

		writer.start_object();
		writer.key("name");
		writer.string(type_name.c_str());
		writer.key("size");
		writer.uint64(static_cast<uint64_t>(type_desc->size));
		writer.key("alignment");
		writer.uint64(static_cast<uint64_t>(type_desc->alignment));
		writer.key("type_category");
		writer.int32(static_cast<int>(type_desc->type_category));

		if (!type_desc->mime_type.empty())
		{
			writer.key("mime_type");
			writer.string(type_desc->mime_type.c_str());
		}

		const EnumDescriptor* enum_desc = type_desc->get_enum_desc();
		if (enum_desc)
		{
			writer.key("enum_values");
			writer.start_array();
			for (const EnumValue& enum_value : enum_desc->values)
			{
				writer.start_object();
				writer.key("name");
				writer.string(enum_value.name.c_str());
				writer.key("value");
				writer.int64(static_cast<int64_t>(enum_value.value));
				writer.end_object();
			}
			writer.end_array();

			writer.key("enum_underlying_size");
			writer.int32(static_cast<int>(enum_desc->underlying_size));
			writer.key("enum_is_signed");
			writer.boolean(enum_desc->is_signed);
			writer.key("enum_is_flags");
			writer.boolean(enum_desc->is_flags);
		}

		const DynamicStructDescriptor* dynamic_struct_desc = type_desc->get_dynamic_struct_desc();
		const StructDescriptor* struct_desc =
			dynamic_struct_desc ? dynamic_struct_desc->get_struct_descriptor(data_ptr) : type_desc->get_struct_desc();

		if (struct_desc)
		{
			writer.key("fields");
			writer.start_array();
			for (const FieldDescriptor& field_desc : struct_desc->fields)
			{
				const TypeDescriptor* field_type = field_desc.find_type_descriptor();
				if (!field_type)
				{
					continue;
				}

				void* field_data_ptr = field_desc.get_data_ptr(data_ptr);
				ROBOTICK_ASSERT(workloads_buffer.contains_object_used_space(field_data_ptr, field_type->size));

				writer.start_object();
				writer.key("name");
				writer.string(field_desc.name.c_str());
				writer.key("offset_within_container");
				writer.int32(static_cast<int>(field_desc.offset_within_container));
				writer.key("type");
				const FixedString256 field_type_name = get_type_name(*field_type, field_data_ptr);
				writer.string(field_type_name.c_str());
				writer.key("element_count");
				writer.int32(static_cast<int>(field_desc.element_count));
				writer.end_object();
			}
			writer.end_array();
		}

		writer.end_object();

		if (struct_desc)
		{
			for (const FieldDescriptor& field_desc : struct_desc->fields)
			{
				const TypeDescriptor* field_type = field_desc.find_type_descriptor();
				if (!field_type)
				{
					continue;
				}

				void* field_data_ptr = field_desc.get_data_ptr(data_ptr);
				ROBOTICK_ASSERT(workloads_buffer.contains_object_used_space(field_data_ptr, field_type->size));
				emit_type_info_stream(writer, workloads_buffer, field_data_ptr, field_type, emitted_type_names);
			}
		}
	}

	template <typename Writer, typename WritableInputFields>
	static void write_layout_json_stream(
		Writer& writer, const Engine& engine, const char* session_id_override, const WritableInputFields& writable_input_fields)
	{
		WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer();
		const auto& instances = engine.get_all_instance_info();
		const auto* workload_stats_type = TypeRegistry::get().find_by_id(GET_TYPE_ID(WorkloadInstanceStats));
		ROBOTICK_ASSERT_MSG(workload_stats_type, "Type 'WorkloadInstanceStats' not registered - this should never happen");

		writer.start_object();
		writer.key("workloads_buffer_size_used");
		writer.uint64(static_cast<uint64_t>(workloads_buffer.get_size_used()));
		writer.key("process_memory_used");
		writer.uint64(static_cast<uint64_t>(get_process_memory_used()));

		writer.key("workloads");
		writer.start_array();
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

			writer.start_object();
			writer.key("name");
			writer.string(workload_instance_info.seed->unique_name.c_str());
			writer.key("type");
			writer.string(workload_instance_info.type->name.c_str());
			writer.key("offset_within_container");
			writer.int32(static_cast<int>(workload_instance_info.offset_in_workloads_buffer));

			void* workload_ptr = (void*)workload_instance_info.get_ptr(workloads_buffer);
			if (desc->config_desc && workload_ptr)
			{
				writer.key("config");
				write_json_struct_ref(writer, desc->config_desc->name.c_str(), desc->config_offset);
			}
			if (desc->inputs_desc && workload_ptr)
			{
				writer.key("inputs");
				write_json_struct_ref(writer, desc->inputs_desc->name.c_str(), desc->inputs_offset);
			}
			if (desc->outputs_desc && workload_ptr)
			{
				writer.key("outputs");
				write_json_struct_ref(writer, desc->outputs_desc->name.c_str(), desc->outputs_offset);
			}

			writer.key("stats_offset_within_container");
			writer.int32(static_cast<int>((uint8_t*)workload_instance_info.workload_stats - workloads_buffer.raw_ptr()));
			writer.end_object();
		}
		writer.end_array();

		const size_t emitted_type_capacity = TypeRegistry::get().get_registered_count() + (instances.size() * 4) + 16;
		EmittedTypeNames emitted_type_names;
		emitted_type_names.initialize(emitted_type_capacity);

		writer.key("types");
		writer.start_array();
		for (const WorkloadInstanceInfo& workload_instance_info : instances)
		{
			if (workload_instance_info.seed == nullptr || workload_instance_info.type == nullptr)
			{
				continue;
			}

			const WorkloadDescriptor* desc = workload_instance_info.type->get_workload_desc();
			if (desc == nullptr)
			{
				continue;
			}

			void* workload_ptr = (void*)workload_instance_info.get_ptr(workloads_buffer);
			if (desc->config_desc && workload_ptr)
			{
				emit_type_info_stream(writer, workloads_buffer, (uint8_t*)workload_ptr + desc->config_offset, desc->config_desc, emitted_type_names);
			}
			if (desc->inputs_desc && workload_ptr)
			{
				emit_type_info_stream(writer, workloads_buffer, (uint8_t*)workload_ptr + desc->inputs_offset, desc->inputs_desc, emitted_type_names);
			}
			if (desc->outputs_desc && workload_ptr)
			{
				emit_type_info_stream(
					writer, workloads_buffer, (uint8_t*)workload_ptr + desc->outputs_offset, desc->outputs_desc, emitted_type_names);
			}

			emit_type_info_stream(writer, workloads_buffer, workload_instance_info.workload_stats, workload_stats_type, emitted_type_names);
		}
		writer.end_array();

		writer.key("engine_session_id");
		writer.string(session_id_override ? session_id_override : "");

		writer.key("writable_inputs");
		writer.start_array();
		for (size_t i = 0; i < writable_input_fields.size(); ++i)
		{
			const auto& writable = writable_input_fields[i];
			writer.start_object();
			writer.key("field_handle");
			writer.uint64(static_cast<uint64_t>(writable.handle));
			writer.key("field_path");
			writer.string(writable.path.c_str());
			writer.key("type");
			writer.string(writable.type_desc ? writable.type_desc->name.c_str() : "unknown");
			writer.key("size");
			writer.uint64(static_cast<uint64_t>(writable.value_size));
			writer.end_object();
		}
		writer.end_array();
		writer.end_object();
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

	void TelemetryServer::Impl::handle_get_workloads_buffer_layout(const WebRequest& /*req*/, WebResponse& res)
	{
		res.set_status_code(WebResponseCode::OK);
		res.set_content_type("application/json");

#if defined(ROBOTICK_PLATFORM_ESP32S3)
		const auto flush_chunk = [&](const char* data, const size_t size)
		{
			res.set_body(data, size);
		};
		json::ChunkedSink<decltype(flush_chunk)> sink(flush_chunk);
		json::Writer<json::ChunkedSink<decltype(flush_chunk)>> writer(sink);
		write_layout_json_stream(writer, *engine, session_id.c_str(), writable_input_fields);
		writer.flush();
#else
		json::StringSink sink;
		json::Writer<json::StringSink> writer(sink);
		write_layout_json_stream(writer, *engine, session_id.c_str(), writable_input_fields);
		writer.flush();
		res.set_body(sink.c_str(), sink.size());
#endif
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

} // namespace robotick
