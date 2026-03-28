// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Sync.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/containers/FixedVector.h"
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
#if defined(ROBOTICK_PLATFORM_ESP32S3)
			char body[4096] = {};
			json::FixedBufferSink sink(body, sizeof(body));
			json::Writer<json::FixedBufferSink> writer(sink);
			build_json(writer);
			writer.flush();
			if (!sink.is_ok())
			{
				res.set_status_code(WebResponseCode::InternalServerError);
				res.set_content_type("application/json");
				res.set_body("{\"error\":\"json_response_overflow\"}", sizeof("{\"error\":\"json_response_overflow\"}") - 1);
				return;
			}
			res.set_status_code(status_code);
			res.set_content_type("application/json");
			res.set_body(sink.c_str(), sink.written_size());
#else
			json::StringSink sink;
			json::Writer<json::StringSink> writer(sink);
			build_json(writer);
			writer.flush();
			res.set_status_code(status_code);
			res.set_content_type("application/json");
			res.set_body(sink.c_str(), sink.size());
#endif
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

		struct TelemetryWriteRequestEntry
		{
			bool has_field_handle = false;
			uint16_t field_handle = 0;
			bool has_field_path = false;
			FixedString512 field_path;
			bool has_value = false;
			FixedString256 value_text;
			bool has_seq = false;
			uint64_t seq = 0;
		};

		struct TelemetryWriteRequestPayload
		{
			TelemetryWriteRequestPayload() { writes.initialize(max_writes); }

			bool has_engine_session_id = false;
			FixedString64 engine_session_id;
			bool has_writes = false;
			static constexpr size_t max_writes = 32;
			HeapVector<TelemetryWriteRequestEntry> writes;
			size_t write_count = 0;
		};

		class JsonCursor
		{
		  public:
			JsonCursor(const char* text, const size_t size)
				: cur(text)
				, end(text ? text + size : nullptr)
			{
			}

			bool parse_write_request(TelemetryWriteRequestPayload& out_payload)
			{
				skip_ws();
				if (!consume('{'))
				{
					return false;
				}

				skip_ws();
				if (consume('}'))
				{
					return true;
				}

				while (!is_at_end())
				{
					FixedString64 key;
					if (!parse_string(key))
					{
						return false;
					}
					skip_ws();
					if (!consume(':'))
					{
						return false;
					}
					skip_ws();

					if (key == "engine_session_id")
					{
						out_payload.has_engine_session_id = parse_string(out_payload.engine_session_id);
						if (!out_payload.has_engine_session_id)
						{
							return false;
						}
					}
					else if (key == "writes")
					{
						out_payload.has_writes = true;
						if (!parse_writes_array(out_payload.writes, out_payload.write_count))
						{
							return false;
						}
					}
					else if (!skip_value())
					{
						return false;
					}

					skip_ws();
					if (consume('}'))
					{
						return true;
					}
					if (!consume(','))
					{
						return false;
					}
					skip_ws();
				}

				return false;
			}

		  private:
			const char* cur = nullptr;
			const char* end = nullptr;

			bool is_at_end() const { return !cur || !end || cur >= end; }

			void skip_ws()
			{
				while (!is_at_end())
				{
					const char ch = *cur;
					if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
					{
						break;
					}
					++cur;
				}
			}

			bool consume(const char expected)
			{
				if (is_at_end() || *cur != expected)
				{
					return false;
				}
				++cur;
				return true;
			}

			bool matches_literal(const char* literal) const
			{
				if (!literal || is_at_end())
				{
					return false;
				}
				const size_t literal_length = ::strlen(literal);
				return static_cast<size_t>(end - cur) >= literal_length && ::strncmp(cur, literal, literal_length) == 0;
			}

			template <size_t N> bool parse_string(FixedString<N>& out)
			{
				if (!consume('"'))
				{
					return false;
				}

				size_t out_len = 0;
				while (!is_at_end())
				{
					const char ch = *cur++;
					if (ch == '"')
					{
						out.data[out_len] = '\0';
						return true;
					}
					if (ch == '\\')
					{
						if (is_at_end())
						{
							return false;
						}
						const char escaped = *cur++;
						char decoded = '\0';
						switch (escaped)
						{
						case '"':
						case '\\':
						case '/':
							decoded = escaped;
							break;
						case 'b':
							decoded = '\b';
							break;
						case 'f':
							decoded = '\f';
							break;
						case 'n':
							decoded = '\n';
							break;
						case 'r':
							decoded = '\r';
							break;
						case 't':
							decoded = '\t';
							break;
						default:
							return false;
						}
						if (out_len + 1 >= N)
						{
							return false;
						}
						out.data[out_len++] = decoded;
						continue;
					}

					if (out_len + 1 >= N)
					{
						return false;
					}
					out.data[out_len++] = ch;
				}

				return false;
			}

			bool parse_integer(int64_t& out_value)
			{
				if (is_at_end())
				{
					return false;
				}

				FixedString32 token;
				size_t token_len = 0;

				if (*cur == '-')
				{
					token.data[token_len++] = *cur++;
				}
				if (is_at_end() || *cur < '0' || *cur > '9')
				{
					return false;
				}
				while (!is_at_end() && *cur >= '0' && *cur <= '9')
				{
					if (token_len + 1 >= token.capacity())
					{
						return false;
					}
					token.data[token_len++] = *cur++;
				}
				token.data[token_len] = '\0';

				long long parsed = 0;
				if (::sscanf(token.c_str(), "%lld", &parsed) != 1)
				{
					return false;
				}
				out_value = static_cast<int64_t>(parsed);
				return true;
			}

			bool parse_number_token(FixedString256& out)
			{
				if (is_at_end())
				{
					return false;
				}

				const char* start = cur;
				if (*cur == '-')
				{
					++cur;
				}
				if (is_at_end() || *cur < '0' || *cur > '9')
				{
					return false;
				}
				if (*cur == '0')
				{
					++cur;
				}
				else
				{
					while (!is_at_end() && *cur >= '0' && *cur <= '9')
					{
						++cur;
					}
				}
				if (!is_at_end() && *cur == '.')
				{
					++cur;
					if (is_at_end() || *cur < '0' || *cur > '9')
					{
						return false;
					}
					while (!is_at_end() && *cur >= '0' && *cur <= '9')
					{
						++cur;
					}
				}
				if (!is_at_end() && (*cur == 'e' || *cur == 'E'))
				{
					++cur;
					if (!is_at_end() && (*cur == '+' || *cur == '-'))
					{
						++cur;
					}
					if (is_at_end() || *cur < '0' || *cur > '9')
					{
						return false;
					}
					while (!is_at_end() && *cur >= '0' && *cur <= '9')
					{
						++cur;
					}
				}

				out.assign(start, static_cast<size_t>(cur - start));
				return true;
			}

			bool skip_string()
			{
				if (!consume('"'))
				{
					return false;
				}
				while (!is_at_end())
				{
					const char ch = *cur++;
					if (ch == '"')
					{
						return true;
					}
					if (ch == '\\')
					{
						if (is_at_end())
						{
							return false;
						}
						++cur;
					}
				}
				return false;
			}

			bool skip_value()
			{
				skip_ws();
				if (is_at_end())
				{
					return false;
				}

				if (*cur == '"')
				{
					return skip_string();
				}
				if (*cur == '{')
				{
					++cur;
					skip_ws();
					if (consume('}'))
					{
						return true;
					}
					while (!is_at_end())
					{
						if (!skip_string())
						{
							return false;
						}
						skip_ws();
						if (!consume(':'))
						{
							return false;
						}
						skip_ws();
						if (!skip_value())
						{
							return false;
						}
						skip_ws();
						if (consume('}'))
						{
							return true;
						}
						if (!consume(','))
						{
							return false;
						}
						skip_ws();
					}
					return false;
				}
				if (*cur == '[')
				{
					++cur;
					skip_ws();
					if (consume(']'))
					{
						return true;
					}
					while (!is_at_end())
					{
						if (!skip_value())
						{
							return false;
						}
						skip_ws();
						if (consume(']'))
						{
							return true;
						}
						if (!consume(','))
						{
							return false;
						}
						skip_ws();
					}
					return false;
				}

				FixedString256 scalar;
				if (parse_number_token(scalar))
				{
					return true;
				}
				if (matches_literal("true"))
				{
					cur += 4;
					return true;
				}
				if (matches_literal("false"))
				{
					cur += 5;
					return true;
				}
				if (matches_literal("null"))
				{
					cur += 4;
					return true;
				}
				return false;
			}

			bool parse_scalar(FixedString256& out)
			{
				skip_ws();
				if (is_at_end())
				{
					return false;
				}
				if (*cur == '"')
				{
					return parse_string(out);
				}
				if (matches_literal("true"))
				{
					out = "true";
					cur += 4;
					return true;
				}
				if (matches_literal("false"))
				{
					out = "false";
					cur += 5;
					return true;
				}
				return parse_number_token(out);
			}

			bool parse_single_write(TelemetryWriteRequestEntry& out_entry)
			{
				if (!consume('{'))
				{
					return false;
				}
				skip_ws();
				if (consume('}'))
				{
					return true;
				}

				while (!is_at_end())
				{
					FixedString64 key;
					if (!parse_string(key))
					{
						return false;
					}
					skip_ws();
					if (!consume(':'))
					{
						return false;
					}
					skip_ws();

					if (key == "field_handle")
					{
						int64_t parsed_handle = 0;
						if (!parse_integer(parsed_handle) || parsed_handle <= 0 || parsed_handle > 0xFFFF)
						{
							return false;
						}
						out_entry.has_field_handle = true;
						out_entry.field_handle = static_cast<uint16_t>(parsed_handle);
					}
					else if (key == "field_path")
					{
						out_entry.has_field_path = parse_string(out_entry.field_path);
						if (!out_entry.has_field_path)
						{
							return false;
						}
					}
					else if (key == "value")
					{
						out_entry.has_value = parse_scalar(out_entry.value_text);
						if (!out_entry.has_value)
						{
							return false;
						}
					}
					else if (key == "seq")
					{
						int64_t parsed_seq = 0;
						if (!parse_integer(parsed_seq))
						{
							return false;
						}
						out_entry.has_seq = true;
						out_entry.seq = parsed_seq > 0 ? static_cast<uint64_t>(parsed_seq) : 0;
					}
					else if (!skip_value())
					{
						return false;
					}

					skip_ws();
					if (consume('}'))
					{
						return true;
					}
					if (!consume(','))
					{
						return false;
					}
					skip_ws();
				}

				return false;
			}

			bool parse_writes_array(HeapVector<TelemetryWriteRequestEntry>& out_writes, size_t& out_count)
			{
				if (!consume('['))
				{
					return false;
				}

				skip_ws();
				if (consume(']'))
				{
					return true;
				}

				while (!is_at_end())
				{
					if (out_count >= out_writes.size())
					{
						return false;
					}

					TelemetryWriteRequestEntry entry;
					if (!parse_single_write(entry))
					{
						return false;
					}
					out_writes[out_count] = entry;
					out_count += 1;

					skip_ws();
					if (consume(']'))
					{
						return true;
					}
					if (!consume(','))
					{
						return false;
					}
					skip_ws();
				}

				return false;
			}
		};

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

		static void reset_forward_http_result(ForwardHttpResult& result)
		{
			result.status_code = WebResponseCode::ServiceUnavailable;
			result.transport_ok = false;
			result.content_type.clear();
			result.session_header.clear();
			result.frame_seq_header.clear();
			result.body.reset();
		}

		static void copy_forward_http_result(const ForwardHttpResult& src, ForwardHttpResult& dst)
		{
			reset_forward_http_result(dst);
			dst.status_code = src.status_code;
			dst.transport_ok = src.transport_ok;
			dst.content_type = src.content_type;
			dst.session_header = src.session_header;
			dst.frame_seq_header = src.frame_seq_header;
			if (src.body.size() > 0)
			{
				dst.body.initialize(src.body.size());
				::memcpy(dst.body.data(), src.body.data(), src.body.size());
			}
		}

		static void apply_forward_http_result_to_response(const ForwardHttpResult& forwarded, WebResponse& res)
		{
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
		}

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
#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
			Mutex forwarded_request_mutex;
			Mutex forwarded_raw_mutex;
			Mutex forwarded_layout_mutex;
			Mutex forwarded_write_mutex;
			ForwardHttpResult cached_raw;
			ForwardHttpResult cached_layout;
			Clock::time_point cached_raw_at{};
			bool has_cached_raw = false;
			bool has_cached_layout = false;
			FixedString64 last_seen_session_id;
			FixedString64 cached_layout_session_id;
#endif
		};

		WebServer web_server;
		const Engine* engine = nullptr;
		FixedString64 session_id;
		HeapVector<WritableInputField> writable_input_fields;
		HeapVector<PendingInputWrite> pending_input_writes;
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		Mutex layout_cache_mutex;
		static constexpr size_t layout_cache_capacity = 16384;
		char cached_layout_body[layout_cache_capacity] = {};
		size_t cached_layout_body_size = 0;
		bool has_cached_layout_body = false;
		bool layout_cache_build_failed = false;
		FixedString64 layout_cache_failure_reason = "not_built";
#endif
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
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		void rebuild_layout_cache();
#endif
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

#if defined(ROBOTICK_PLATFORM_ESP32S3)
		impl->rebuild_layout_cache();
#endif

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
		const auto forwarded_raw_cache_duration = Clock::from_seconds(0.1);

		FixedString256 url;
		url.format("http://%s:%u/api/telemetry%s", peer.host.c_str(), static_cast<unsigned int>(peer.telemetry_port), suffix_uri);

		ForwardHttpResult forwarded;
		const bool is_get = req.method.equals("GET");
		const bool is_post = req.method.equals("POST");
		const bool is_layout_request = is_get && StringView(suffix_uri).equals("/workloads_buffer/layout");
		const bool is_raw_request = is_get && StringView(suffix_uri).equals("/workloads_buffer/raw");
		const bool is_write_request = is_post && StringView(suffix_uri).equals("/set_workload_input_fields_data");

		auto forward_or_error = [&](ForwardHttpResult& out_result) -> bool
		{
			LockGuard request_lock(const_cast<Mutex&>(peer.forwarded_request_mutex));
			return perform_forwarded_http_request(req.method.c_str(), url.c_str(), req, out_result) && out_result.transport_ok;
		};

		if (is_layout_request)
		{
			LockGuard lock(const_cast<Mutex&>(peer.forwarded_layout_mutex));
			if (peer.has_cached_layout)
			{
				apply_forward_http_result_to_response(peer.cached_layout, res);
				return;
			}
			if (!forward_or_error(forwarded))
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

			copy_forward_http_result(forwarded, const_cast<ForwardHttpResult&>(peer.cached_layout));
			const_cast<TelemetryPeerRoute&>(peer).has_cached_layout = true;
			const_cast<TelemetryPeerRoute&>(peer).cached_layout_session_id = peer.last_seen_session_id;
			apply_forward_http_result_to_response(forwarded, res);
			return;
		}

		if (is_raw_request)
		{
			LockGuard lock(const_cast<Mutex&>(peer.forwarded_raw_mutex));
			const Clock::time_point now = Clock::now();
			if (peer.has_cached_raw && (now - peer.cached_raw_at) < forwarded_raw_cache_duration)
			{
				apply_forward_http_result_to_response(peer.cached_raw, res);
				return;
			}

			UniqueLock request_lock(const_cast<Mutex&>(peer.forwarded_request_mutex), std_approved::defer_lock);
			if (!request_lock.try_lock())
			{
				if (peer.has_cached_raw)
				{
					apply_forward_http_result_to_response(peer.cached_raw, res);
					return;
				}
				request_lock.lock();
			}

			if (!(perform_forwarded_http_request(req.method.c_str(), url.c_str(), req, forwarded) && forwarded.transport_ok))
			{
				if (peer.has_cached_raw)
				{
					apply_forward_http_result_to_response(peer.cached_raw, res);
					return;
				}
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

			TelemetryPeerRoute& mutable_peer = const_cast<TelemetryPeerRoute&>(peer);
			if (!forwarded.session_header.empty())
			{
				if (!mutable_peer.last_seen_session_id.empty() && mutable_peer.last_seen_session_id != forwarded.session_header)
				{
					reset_forward_http_result(mutable_peer.cached_layout);
					mutable_peer.has_cached_layout = false;
					mutable_peer.cached_layout_session_id.clear();
				}
				mutable_peer.last_seen_session_id = forwarded.session_header;
			}
			copy_forward_http_result(forwarded, mutable_peer.cached_raw);
			mutable_peer.cached_raw_at = now;
			mutable_peer.has_cached_raw = true;
			if (!mutable_peer.cached_layout_session_id.empty() && !mutable_peer.last_seen_session_id.empty() &&
				mutable_peer.cached_layout_session_id != mutable_peer.last_seen_session_id)
			{
				reset_forward_http_result(mutable_peer.cached_layout);
				mutable_peer.has_cached_layout = false;
				mutable_peer.cached_layout_session_id.clear();
			}
			apply_forward_http_result_to_response(forwarded, res);
			return;
		}

		if (is_write_request)
		{
			LockGuard lock(const_cast<Mutex&>(peer.forwarded_write_mutex));
			if (!forward_or_error(forwarded))
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
			apply_forward_http_result_to_response(forwarded, res);
			return;
		}

		if (!forward_or_error(forwarded))
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
		apply_forward_http_result_to_response(forwarded, res);
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

		TelemetryWriteRequestPayload payload;

		JsonCursor cursor(reinterpret_cast<const char*>(req.body.data()), req.body.size());
		if (!cursor.parse_write_request(payload))
		{
			set_error_response(res, WebResponseCode::BadRequest, "invalid_json");
			return;
		}

		const bool session_matches = !payload.has_engine_session_id || StringView(session_id.c_str()).equals(payload.engine_session_id.c_str());

		if (!payload.has_writes || payload.write_count == 0)
		{
			set_error_response(res, WebResponseCode::BadRequest, "missing_writes");
			return;
		}

		HeapVector<ParsedWrite> parsed_writes;
		parsed_writes.initialize(payload.write_count);
		size_t parsed_count = 0;

		bool parse_failed = false;
		int parse_failed_status = WebResponseCode::BadRequest;
		const char* parse_failed_error = nullptr;

		for (size_t write_payload_index = 0; write_payload_index < payload.write_count; ++write_payload_index)
		{
			const TelemetryWriteRequestEntry& write_payload = payload.writes[write_payload_index];
			if (parse_failed)
			{
				break;
			}

			if (!session_matches && !write_payload.has_field_path)
			{
				parse_failed = true;
				parse_failed_status = WebResponseCode::PreconditionFailed;
				parse_failed_error = "field_path_required_for_stale_session_write";
				break;
			}

			int writable_index = -1;
			uint16_t writable_handle = 0;
			FixedString512 requested_path;

			if (write_payload.has_field_handle)
			{
				writable_handle = write_payload.field_handle;
				writable_index = find_writable_input_index_by_handle(writable_handle);
			}

			if (write_payload.has_field_path)
			{
				requested_path = write_payload.field_path.c_str();
				const int path_index = find_writable_input_index_by_path(requested_path.c_str());
				if (path_index >= 0)
				{
					if (writable_index >= 0 && static_cast<size_t>(writable_index) != static_cast<size_t>(path_index))
					{
						parse_failed = true;
						parse_failed_error = "field_handle_path_mismatch";
						break;
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
				break;
			}

			if (!write_payload.has_value)
			{
				parse_failed = true;
				parse_failed_error = "missing_value";
				break;
			}

			WritableInputField& writable = writable_input_fields[static_cast<size_t>(writable_index)];
			if (!writable.type_desc || writable.value_size == 0 || writable.value_size > kMaxWritePayloadBytes)
			{
				parse_failed = true;
				parse_failed_error = "unsupported_target_field";
				break;
			}

			ParsedWrite& parsed = parsed_writes[parsed_count];
			parsed = ParsedWrite{};
			parsed.writable_index = static_cast<size_t>(writable_index);
			parsed.writable_handle = writable_handle;
			parsed.staged.pending = true;
			parsed.staged.payload_size = writable.value_size;
			::memset(parsed.staged.payload, 0, sizeof(parsed.staged.payload));
			if (!writable.type_desc->from_string(write_payload.value_text.c_str(), parsed.staged.payload))
			{
				parse_failed = true;
				parse_failed_error = "value_parse_failed";
				break;
			}

			if (write_payload.has_seq)
			{
				parsed.staged.seq = write_payload.seq;
			}

			parsed_count += 1;
		}

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
				PendingInputWrite& pending = pending_input_writes[parsed.writable_index];
				if (parsed.staged.seq == 0)
				{
					const uint64_t next_seq = max(write_seq_counter, pending.seq) + 1;
					write_seq_counter = next_seq;
					parsed.staged.seq = next_seq;
				}
				else if (parsed.staged.seq > write_seq_counter)
				{
					write_seq_counter = parsed.staged.seq;
				}

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
				writer.key("engine_session_id");
				writer.string(session_id.c_str());
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
		static constexpr size_t capacity = 512;
		FixedVector<uint32_t, capacity> hashes;

		void clear() { hashes.clear(); }

		bool contains(const char* name) const
		{
			const uint32_t target_hash = hash_string(name ? name : "");
			for (size_t i = 0; i < hashes.size(); ++i)
			{
				if (hashes[i] == target_hash)
				{
					return true;
				}
			}
			return false;
		}

		void add(const char* name)
		{
			ROBOTICK_ASSERT_MSG(!hashes.full(), "Telemetry layout emitted type capacity exceeded");
			hashes.add(hash_string(name ? name : ""));
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

		EmittedTypeNames emitted_type_names;
		emitted_type_names.clear();

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

#if defined(ROBOTICK_PLATFORM_ESP32S3)
	void TelemetryServer::Impl::rebuild_layout_cache()
	{
		if (!engine)
		{
			LockGuard lock(layout_cache_mutex);
			has_cached_layout_body = false;
			cached_layout_body_size = 0;
			cached_layout_body[0] = '\0';
			layout_cache_build_failed = true;
			layout_cache_failure_reason = "no_engine";
			return;
		}

		char new_cached_layout_body[layout_cache_capacity] = {};
		json::FixedBufferSink sink(new_cached_layout_body, layout_cache_capacity);
		json::Writer<json::FixedBufferSink> writer(sink);
		write_layout_json_stream(writer, *engine, session_id.c_str(), writable_input_fields);
		writer.flush();

		if (!sink.is_ok())
		{
			LockGuard lock(layout_cache_mutex);
			has_cached_layout_body = false;
			cached_layout_body_size = 0;
			cached_layout_body[0] = '\0';
			layout_cache_build_failed = true;
			layout_cache_failure_reason = "buffer_overflow";
			return;
		}

		new_cached_layout_body[sink.written_size()] = '\0';

		LockGuard lock(layout_cache_mutex);
		::memcpy(cached_layout_body, new_cached_layout_body, sink.written_size() + 1);
		cached_layout_body_size = sink.written_size();
		has_cached_layout_body = true;
		layout_cache_build_failed = false;
		layout_cache_failure_reason = "";
	}
#endif

	void TelemetryServer::Impl::handle_get_workloads_buffer_layout(const WebRequest& /*req*/, WebResponse& res)
	{
		res.set_status_code(WebResponseCode::OK);
		res.set_content_type("application/json");

#if defined(ROBOTICK_PLATFORM_ESP32S3)
		LockGuard lock(layout_cache_mutex);
		if (!has_cached_layout_body)
		{
			set_json_response(res,
				WebResponseCode::ServiceUnavailable,
				[&](auto& writer)
				{
					writer.start_object();
					writer.key("error");
					writer.string("telemetry_layout_generation_failed");
					writer.key("reason");
					writer.string(layout_cache_failure_reason.empty() ? "not_built" : layout_cache_failure_reason.c_str());
					writer.end_object();
				});
			return;
		}
		res.set_body(cached_layout_body, cached_layout_body_size);
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
