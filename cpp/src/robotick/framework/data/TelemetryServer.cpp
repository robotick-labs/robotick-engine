// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/TelemetryServer.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Atomic.h"
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
#include <arpa/inet.h>
#include <curl/curl.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
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

		static bool ends_with(const char* text, const char* suffix)
		{
			if (!text || !suffix)
			{
				return false;
			}
			const size_t text_len = ::strlen(text);
			const size_t suffix_len = ::strlen(suffix);
			return text_len >= suffix_len && ::strncmp(text + (text_len - suffix_len), suffix, suffix_len) == 0;
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

		struct TelemetryConnectionStateRequestEntry
		{
			bool has_field_handle = false;
			uint16_t field_handle = 0;
			bool has_field_path = false;
			FixedString512 field_path;
			bool has_enabled = false;
			bool enabled = true;
		};

		struct TelemetryConnectionStateRequestPayload
		{
			TelemetryConnectionStateRequestPayload() { updates.initialize(max_updates); }

			bool has_engine_session_id = false;
			FixedString64 engine_session_id;
			bool has_updates = false;
			static constexpr size_t max_updates = 32;
			HeapVector<TelemetryConnectionStateRequestEntry> updates;
			size_t update_count = 0;
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

			bool parse_connection_state_request(TelemetryConnectionStateRequestPayload& out_payload)
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
					else if (key == "updates")
					{
						out_payload.has_updates = true;
						if (!parse_connection_updates_array(out_payload.updates, out_payload.update_count))
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

			bool parse_bool(bool& out)
			{
				if (matches_literal("true"))
				{
					out = true;
					cur += 4;
					return true;
				}
				if (matches_literal("false"))
				{
					out = false;
					cur += 5;
					return true;
				}
				return false;
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

			bool parse_single_connection_update(TelemetryConnectionStateRequestEntry& out_entry)
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
					else if (key == "enabled")
					{
						out_entry.has_enabled = parse_bool(out_entry.enabled);
						if (!out_entry.has_enabled)
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

			bool parse_connection_updates_array(HeapVector<TelemetryConnectionStateRequestEntry>& out_updates, size_t& out_count)
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
					if (out_count >= out_updates.size())
					{
						return false;
					}

					TelemetryConnectionStateRequestEntry entry;
					if (!parse_single_connection_update(entry))
					{
						return false;
					}
					out_updates[out_count] = entry;
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

		static void close_socket_fd(int& fd)
		{
			if (fd >= 0)
			{
				::close(fd);
				fd = -1;
			}
		}

		static bool socket_wait_readable(const int fd, const int timeout_ms)
		{
			if (fd < 0)
			{
				return false;
			}
			fd_set read_set;
			FD_ZERO(&read_set);
			FD_SET(fd, &read_set);

			timeval timeout{};
			timeout.tv_sec = timeout_ms > 0 ? timeout_ms / 1000 : 0;
			timeout.tv_usec = timeout_ms > 0 ? (timeout_ms % 1000) * 1000 : 0;

			const int rc = ::select(fd + 1, &read_set, nullptr, nullptr, &timeout);
			return rc > 0 && FD_ISSET(fd, &read_set);
		}

		static bool socket_send_all(const int fd, const uint8_t* data, const size_t size)
		{
			size_t sent = 0;
			while (sent < size)
			{
				const ssize_t written = ::send(fd, data + sent, size - sent, 0);
				if (written <= 0)
				{
					return false;
				}
				sent += static_cast<size_t>(written);
			}
			return true;
		}

		static bool socket_recv_exact_with_timeout(const int fd, uint8_t* out, const size_t size, const int timeout_ms)
		{
			size_t read = 0;
			while (read < size)
			{
				if (!socket_wait_readable(fd, timeout_ms))
				{
					return false;
				}
				const ssize_t n = ::recv(fd, out + read, size - read, 0);
				if (n <= 0)
				{
					return false;
				}
				read += static_cast<size_t>(n);
			}
			return true;
		}

		static bool ws_send_masked_frame(const int fd, const int opcode, const void* data, const size_t size)
		{
			if (fd < 0)
			{
				return false;
			}
			if (size > 0xFFFF)
			{
				return false;
			}

			uint8_t header[14] = {};
			size_t header_size = 0;
			header[header_size++] = static_cast<uint8_t>(0x80 | (opcode & 0x0F));
			if (size <= 125)
			{
				header[header_size++] = static_cast<uint8_t>(0x80 | size);
			}
			else
			{
				header[header_size++] = static_cast<uint8_t>(0x80 | 126);
				header[header_size++] = static_cast<uint8_t>((size >> 8) & 0xFF);
				header[header_size++] = static_cast<uint8_t>(size & 0xFF);
			}

			const uint8_t mask[4] = {0x13, 0x57, 0x9B, 0xDF};
			header[header_size++] = mask[0];
			header[header_size++] = mask[1];
			header[header_size++] = mask[2];
			header[header_size++] = mask[3];

			if (!socket_send_all(fd, header, header_size))
			{
				return false;
			}

			if (size == 0)
			{
				return true;
			}

			HeapVector<uint8_t> masked_payload;
			masked_payload.initialize(size);
			const uint8_t* payload = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; ++i)
			{
				const uint8_t src = payload ? payload[i] : 0;
				masked_payload[i] = src ^ mask[i % 4];
			}

			return socket_send_all(fd, masked_payload.data(), size);
		}

		static bool ws_send_masked_text(const int fd, const char* text, const size_t size)
		{
			return ws_send_masked_frame(fd, 0x1, text, size);
		}

		static bool ws_send_masked_pong(const int fd, const uint8_t* payload, const size_t size)
		{
			return ws_send_masked_frame(fd, 0xA, payload, size);
		}

		static bool ws_try_read_frame(const int fd, int& out_opcode, HeapVector<uint8_t>& out_payload, size_t& out_payload_size, const int timeout_ms)
		{
			out_opcode = 0;
			out_payload_size = 0;
			out_payload.reset();

			if (!socket_wait_readable(fd, timeout_ms))
			{
				return false;
			}

			uint8_t header[2] = {};
			if (!socket_recv_exact_with_timeout(fd, header, sizeof(header), timeout_ms))
			{
				return false;
			}

			out_opcode = header[0] & 0x0F;
			const bool masked = (header[1] & 0x80) != 0;
			uint64_t payload_size = static_cast<uint64_t>(header[1] & 0x7F);
			if (payload_size == 126)
			{
				uint8_t ext[2] = {};
				if (!socket_recv_exact_with_timeout(fd, ext, sizeof(ext), timeout_ms))
				{
					return false;
				}
				payload_size = (static_cast<uint64_t>(ext[0]) << 8) | static_cast<uint64_t>(ext[1]);
			}
			else if (payload_size == 127)
			{
				uint8_t ext[8] = {};
				if (!socket_recv_exact_with_timeout(fd, ext, sizeof(ext), timeout_ms))
				{
					return false;
				}
				payload_size = 0;
				for (size_t i = 0; i < sizeof(ext); ++i)
				{
					payload_size = (payload_size << 8) | static_cast<uint64_t>(ext[i]);
				}
			}

			if (payload_size > (8U * 1024U * 1024U))
			{
				return false;
			}

			uint8_t mask[4] = {};
			if (masked)
			{
				if (!socket_recv_exact_with_timeout(fd, mask, sizeof(mask), timeout_ms))
				{
					return false;
				}
			}

			if (payload_size > 0)
			{
				out_payload.initialize(static_cast<size_t>(payload_size));
				if (!socket_recv_exact_with_timeout(fd, out_payload.data(), static_cast<size_t>(payload_size), timeout_ms))
				{
					return false;
				}
				if (masked)
				{
					for (size_t i = 0; i < static_cast<size_t>(payload_size); ++i)
					{
						out_payload[i] ^= mask[i % 4];
					}
				}
			}

			out_payload_size = static_cast<size_t>(payload_size);
			return true;
		}

		static bool connect_websocket_client_socket(const char* host, const uint16_t port, const char* path, int& out_fd)
		{
			out_fd = -1;
			if (!host || host[0] == '\0' || !path || path[0] != '/')
			{
				return false;
			}

			char port_text[16];
			::snprintf(port_text, sizeof(port_text), "%u", static_cast<unsigned>(port));

			addrinfo hints{};
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			addrinfo* results = nullptr;
			if (::getaddrinfo(host, port_text, &hints, &results) != 0)
			{
				return false;
			}

			int fd = -1;
			for (addrinfo* it = results; it != nullptr; it = it->ai_next)
			{
				fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
				if (fd < 0)
				{
					continue;
				}
				if (::connect(fd, it->ai_addr, static_cast<socklen_t>(it->ai_addrlen)) == 0)
				{
					break;
				}
				close_socket_fd(fd);
			}
			::freeaddrinfo(results);
			if (fd < 0)
			{
				return false;
			}

			const char* handshake = "GET %s HTTP/1.1\r\n"
									"Host: %s:%u\r\n"
									"Upgrade: websocket\r\n"
									"Connection: Upgrade\r\n"
									"Sec-WebSocket-Version: 13\r\n"
									"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
									"\r\n";

			char request[1024];
			::snprintf(request, sizeof(request), handshake, path, host, static_cast<unsigned>(port));
			if (!socket_send_all(fd, reinterpret_cast<const uint8_t*>(request), ::strlen(request)))
			{
				close_socket_fd(fd);
				return false;
			}

			char response[4096] = {};
			size_t response_len = 0;
			while (response_len + 1 < sizeof(response))
			{
				uint8_t ch = 0;
				if (!socket_recv_exact_with_timeout(fd, &ch, 1, 200))
				{
					close_socket_fd(fd);
					return false;
				}
				response[response_len++] = static_cast<char>(ch);
				response[response_len] = '\0';
				if (::strstr(response, "\r\n\r\n") != nullptr)
				{
					break;
				}
			}

			if (::strstr(response, " 101 ") == nullptr || ::strstr(response, "Sec-WebSocket-Accept:") == nullptr)
			{
				close_socket_fd(fd);
				return false;
			}

			out_fd = fd;
			return true;
		}

		static bool json_extract_uint32(const char* json, const char* key, uint32_t& out_value)
		{
			if (!json || !key)
			{
				return false;
			}
			FixedString64 pattern;
			pattern.format("\"%s\":", key);
			const char* p = ::strstr(json, pattern.c_str());
			if (!p)
			{
				return false;
			}
			p += ::strlen(pattern.c_str());
			while (*p == ' ' || *p == '\t')
			{
				++p;
			}
			unsigned parsed = 0;
			if (::sscanf(p, "%u", &parsed) != 1)
			{
				return false;
			}
			out_value = static_cast<uint32_t>(parsed);
			return true;
		}

		static bool json_extract_string(const char* json, const char* key, FixedString64& out_value)
		{
			if (!json || !key)
			{
				return false;
			}
			FixedString64 pattern;
			pattern.format("\"%s\":\"", key);
			const char* p = ::strstr(json, pattern.c_str());
			if (!p)
			{
				return false;
			}
			p += ::strlen(pattern.c_str());
			const char* end = ::strchr(p, '"');
			if (!end || end <= p)
			{
				return false;
			}
			out_value.assign(p, static_cast<size_t>(end - p));
			return true;
		}
#endif
	} // namespace

	template <typename Writer, typename WritableInputFields>
	static void write_layout_json_stream(
		Writer& writer, const Engine& engine, const char* session_id_override, const WritableInputFields& writable_input_fields);

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
			DataConnectionInputHandle* incoming_connection_handle = nullptr;
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
			int peer_ws_fd = -1;
			Clock::time_point peer_ws_last_connect_attempt{};
			bool peer_ws_has_pending_frame_meta = false;
			uint32_t peer_ws_pending_frame_seq = 0;
			uint32_t peer_ws_pending_payload_size = 0;
			FixedString64 peer_ws_pending_session_id;
			HeapVector<uint8_t> peer_ws_latest_raw;
			uint32_t peer_ws_latest_frame_seq = 0;
			FixedString64 peer_ws_latest_session_id;
			bool peer_ws_has_latest_raw = false;
			HeapVector<char> peer_ws_latest_layout;
			size_t peer_ws_latest_layout_size = 0;
			bool peer_ws_has_latest_layout = false;
			HeapVector<char> peer_ws_write_response;
			size_t peer_ws_write_response_size = 0;
			bool peer_ws_has_write_response = false;
#endif
		};

		struct WsRoute
		{
			bool valid = false;
			bool is_local = true;
			int peer_index = -1;
			FixedString64 model_id;
		};

		struct WsClient
		{
			bool active = false;
			bool ready = false;
			WebSocketConnection connection;
			WsRoute route;
			Clock::time_point last_heartbeat_at{};
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
		Mutex ws_clients_mutex;
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		static constexpr size_t ws_max_clients = 1;
#else
		static constexpr size_t ws_max_clients = 8;
#endif
		HeapVector<WsClient> ws_clients;
		AtomicFlag ws_broadcast_stop{false};
		Thread ws_broadcast_thread;
		uint32_t ws_last_local_frame_seq = 0;
		HeapVector<uint32_t> ws_last_peer_frame_seq;
		size_t last_known_writable_connection_handle_count = 0;
		static constexpr uint32_t ws_heartbeat_interval_ms = 1000;
		static constexpr uint32_t ws_broadcast_tick_ms = 10;

		void rebuild_writable_input_registry();
		bool refresh_writable_input_connection_handles();
		size_t count_writable_input_connection_handles() const;
		int find_writable_input_index_by_handle(uint16_t handle) const;
		int find_writable_input_index_by_path(const char* path) const;
		int find_telemetry_peer_index_by_model_id(const char* model_id) const;
		void rebuild_telemetry_peer_registry();
		void initialize_ws_state();
		void clear_ws_clients();
		void configure_websocket_endpoints();
		bool resolve_ws_route(const char* uri, WsRoute& out_route) const;
		bool ws_connections_equal(const WebSocketConnection& lhs, const WebSocketConnection& rhs) const;
		int find_ws_client_index_unlocked(const WebSocketConnection& connection) const;
		bool register_ws_client(const WsRoute& route, const WebSocketConnection& incoming, int& out_index);
		void unregister_ws_client(const WebSocketConnection& connection);
		void send_ws_hello(const WsClient& client);
		void send_ws_layout(const WsClient& client);
		void send_ws_error(const WebSocketConnection& connection, const char* error_text);
		bool handle_ws_message(const WsRoute& route, const WebSocketConnection& connection, const WebSocketMessage& message);
		void build_write_response_json(const char* request_body, size_t request_size, int& out_status_code, json::StringSink& out_json);
		void build_connection_state_response_json(const char* request_body, size_t request_size, int& out_status_code, json::StringSink& out_json);
		void maybe_broadcast_local_layout_update();
		void maybe_broadcast_local_frame(const Clock::time_point now);
		void maybe_broadcast_peer_frames(const Clock::time_point now);
		void maybe_send_heartbeats(const Clock::time_point now);
		void start_ws_broadcast_loop();
		void stop_ws_broadcast_loop();
		static void ws_broadcast_entry(void* user_data);
		void ws_broadcast_loop();
#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		void reset_peer_ws_state(TelemetryPeerRoute& peer);
		void close_peer_ws_connection(TelemetryPeerRoute& peer);
		bool ensure_peer_ws_connection(TelemetryPeerRoute& peer, int timeout_ms);
		bool pump_peer_ws_messages(TelemetryPeerRoute& peer, int max_frames, int timeout_ms);
		bool wait_for_peer_layout(TelemetryPeerRoute& peer, int timeout_ms);
		bool wait_for_peer_write_response(TelemetryPeerRoute& peer, int timeout_ms);
#endif
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		void rebuild_layout_cache();
#endif
		bool handle_local_telemetry_request(const WebRequest& req, WebResponse& res, const char* effective_uri);
		void handle_get_gateway_models(WebResponse& res);
		void handle_forwarded_telemetry_request(const WebRequest& req, WebResponse& res, const TelemetryPeerRoute& peer, const char* suffix_uri);
		void handle_get_workloads_buffer_layout(const WebRequest& req, WebResponse& res);
		void handle_get_workloads_buffer_raw(const WebRequest& req, WebResponse& res);
		void handle_set_workload_input_fields_data(const WebRequest& req, WebResponse& res);
		void handle_set_workload_input_connection_state(const WebRequest& req, WebResponse& res);
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
			impl->stop_ws_broadcast_loop();
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
		impl->initialize_ws_state();
		impl->is_setup = true;
	}

	void TelemetryServer::start(const Engine& engine_in, const uint16_t telemetry_port)
	{
		if (!impl || !impl->is_setup)
		{
			setup(engine_in);
		}
		impl->engine = &engine_in;
		impl->configure_websocket_endpoints();

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
						const int peer_index = impl->find_telemetry_peer_index_by_model_id(routed_model_id.c_str());
						if (peer_index < 0)
						{
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

						impl->handle_forwarded_telemetry_request(
							req, res, impl->telemetry_peers[static_cast<size_t>(peer_index)], routed_suffix_uri.c_str());
						return true;
					}
				}

				return impl->handle_local_telemetry_request(req, res, req.uri.c_str());
			});
		impl->start_ws_broadcast_loop();
	}

	void TelemetryServer::stop()
	{
		if (!impl)
		{
			return;
		}
		impl->stop_ws_broadcast_loop();
#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		for (size_t i = 0; i < impl->telemetry_peers.size(); ++i)
		{
			LockGuard lock(impl->telemetry_peers[i].forwarded_request_mutex);
			impl->close_peer_ws_connection(impl->telemetry_peers[i]);
		}
#endif
		impl->web_server.stop();
		impl->engine = nullptr;
		impl->clear_ws_clients();
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
#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		{
			LockGuard lock(peer.forwarded_request_mutex);
			impl->close_peer_ws_connection(peer);
		}
#endif
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

	void TelemetryServer::Impl::initialize_ws_state()
	{
		if (ws_clients.empty())
		{
			ws_clients.initialize(ws_max_clients);
		}
		for (size_t i = 0; i < ws_clients.size(); ++i)
		{
			ws_clients[i] = WsClient{};
		}

		if (!telemetry_peers.empty() && ws_last_peer_frame_seq.empty())
		{
			ws_last_peer_frame_seq.initialize(telemetry_peers.size());
		}
		for (size_t i = 0; i < ws_last_peer_frame_seq.size(); ++i)
		{
			ws_last_peer_frame_seq[i] = 0;
		}

		ws_last_local_frame_seq = 0;
		ws_broadcast_stop.clear();
	}

	void TelemetryServer::Impl::clear_ws_clients()
	{
		LockGuard lock(ws_clients_mutex);
		for (size_t i = 0; i < ws_clients.size(); ++i)
		{
			ws_clients[i] = WsClient{};
		}
	}

#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
	void TelemetryServer::Impl::reset_peer_ws_state(TelemetryPeerRoute& peer)
	{
		peer.peer_ws_has_pending_frame_meta = false;
		peer.peer_ws_pending_frame_seq = 0;
		peer.peer_ws_pending_payload_size = 0;
		peer.peer_ws_pending_session_id.clear();
		peer.peer_ws_latest_raw.reset();
		peer.peer_ws_latest_frame_seq = 0;
		peer.peer_ws_latest_session_id.clear();
		peer.peer_ws_has_latest_raw = false;
		peer.peer_ws_latest_layout.reset();
		peer.peer_ws_latest_layout_size = 0;
		peer.peer_ws_has_latest_layout = false;
		peer.peer_ws_write_response.reset();
		peer.peer_ws_write_response_size = 0;
		peer.peer_ws_has_write_response = false;
	}

	void TelemetryServer::Impl::close_peer_ws_connection(TelemetryPeerRoute& peer)
	{
		close_socket_fd(peer.peer_ws_fd);
		reset_peer_ws_state(peer);
	}

	bool TelemetryServer::Impl::ensure_peer_ws_connection(TelemetryPeerRoute& peer, const int timeout_ms)
	{
		if (peer.peer_ws_fd >= 0)
		{
			return true;
		}

		const Clock::time_point now = Clock::now();
		if (peer.peer_ws_last_connect_attempt.time_since_epoch().count() != 0)
		{
			const uint64_t elapsed_ms = static_cast<uint64_t>(Clock::to_nanoseconds(now - peer.peer_ws_last_connect_attempt).count() / 1000000ULL);
			if (elapsed_ms < 200)
			{
				return false;
			}
		}
		peer.peer_ws_last_connect_attempt = now;

		int fd = -1;
		if (!connect_websocket_client_socket(peer.host.c_str(), peer.telemetry_port, "/api/telemetry/ws", fd))
		{
			close_socket_fd(fd);
			return false;
		}
		peer.peer_ws_fd = fd;
		reset_peer_ws_state(peer);
		return pump_peer_ws_messages(peer, 8, timeout_ms);
	}

	bool TelemetryServer::Impl::pump_peer_ws_messages(TelemetryPeerRoute& peer, const int max_frames, const int timeout_ms)
	{
		if (peer.peer_ws_fd < 0)
		{
			return false;
		}

		for (int i = 0; i < max_frames; ++i)
		{
			int opcode = 0;
			HeapVector<uint8_t> payload;
			size_t payload_size = 0;
			if (!ws_try_read_frame(peer.peer_ws_fd, opcode, payload, payload_size, i == 0 ? timeout_ms : 0))
			{
				return true;
			}

			if (opcode == 0x9)
			{
				if (!ws_send_masked_pong(peer.peer_ws_fd, payload_size > 0 ? payload.data() : nullptr, payload_size))
				{
					close_peer_ws_connection(peer);
					return false;
				}
				continue;
			}
			if (opcode == 0xA)
			{
				continue;
			}
			if (opcode == 0x8)
			{
				close_peer_ws_connection(peer);
				return false;
			}

			if (opcode == 0x1)
			{
				HeapVector<char> text;
				text.initialize(payload_size + 1);
				if (payload_size > 0)
				{
					::memcpy(text.data(), payload.data(), payload_size);
				}
				text[payload_size] = '\0';

				const char* body = text.data();
				if (::strstr(body, "\"type\":\"heartbeat\""))
				{
					continue;
				}
				if (::strstr(body, "\"type\":\"frame\""))
				{
					uint32_t frame_seq = 0;
					uint32_t payload_len = 0;
					FixedString64 sid;
					if (json_extract_uint32(body, "frame_seq", frame_seq))
					{
						peer.peer_ws_has_pending_frame_meta = true;
						peer.peer_ws_pending_frame_seq = frame_seq;
						peer.peer_ws_pending_payload_size = json_extract_uint32(body, "payload_size", payload_len) ? payload_len : 0;
						if (json_extract_string(body, "engine_session_id", sid))
						{
							peer.peer_ws_pending_session_id = sid.c_str();
						}
						else
						{
							peer.peer_ws_pending_session_id.clear();
						}
					}
					continue;
				}
				if (::strstr(body, "\"type\":\"hello\""))
				{
					continue;
				}
				if (::strstr(body, "\"workloads\"") && ::strstr(body, "\"types\""))
				{
					peer.peer_ws_latest_layout.reset();
					peer.peer_ws_latest_layout.initialize(payload_size + 1);
					if (payload_size > 0)
					{
						::memcpy(peer.peer_ws_latest_layout.data(), body, payload_size);
					}
					peer.peer_ws_latest_layout[payload_size] = '\0';
					peer.peer_ws_latest_layout_size = payload_size;
					peer.peer_ws_has_latest_layout = true;
					continue;
				}
				if (::strstr(body, "\"accepted_count\"") || ::strstr(body, "\"ignored_stale_count\"") || ::strstr(body, "\"error\""))
				{
					peer.peer_ws_write_response.reset();
					peer.peer_ws_write_response.initialize(payload_size + 1);
					if (payload_size > 0)
					{
						::memcpy(peer.peer_ws_write_response.data(), body, payload_size);
					}
					peer.peer_ws_write_response[payload_size] = '\0';
					peer.peer_ws_write_response_size = payload_size;
					peer.peer_ws_has_write_response = true;
				}
				continue;
			}

			if (opcode == 0x2 && peer.peer_ws_has_pending_frame_meta)
			{
				peer.peer_ws_latest_raw.reset();
				peer.peer_ws_latest_raw.initialize(payload_size);
				if (payload_size > 0)
				{
					::memcpy(peer.peer_ws_latest_raw.data(), payload.data(), payload_size);
				}
				peer.peer_ws_latest_frame_seq = peer.peer_ws_pending_frame_seq;
				peer.peer_ws_latest_session_id = peer.peer_ws_pending_session_id.c_str();
				peer.peer_ws_has_latest_raw = true;
				peer.peer_ws_has_pending_frame_meta = false;
				peer.peer_ws_pending_frame_seq = 0;
				peer.peer_ws_pending_payload_size = 0;
				peer.peer_ws_pending_session_id.clear();
			}
		}

		return true;
	}

	bool TelemetryServer::Impl::wait_for_peer_layout(TelemetryPeerRoute& peer, const int timeout_ms)
	{
		const Clock::time_point start = Clock::now();
		while (true)
		{
			if (peer.peer_ws_has_latest_layout)
			{
				return true;
			}
			if (!pump_peer_ws_messages(peer, 8, 50))
			{
				return false;
			}
			const int elapsed_ms = static_cast<int>(Clock::to_nanoseconds(Clock::now() - start).count() / 1000000LL);
			if (elapsed_ms >= timeout_ms)
			{
				return false;
			}
		}
	}

	bool TelemetryServer::Impl::wait_for_peer_write_response(TelemetryPeerRoute& peer, const int timeout_ms)
	{
		const Clock::time_point start = Clock::now();
		while (true)
		{
			if (peer.peer_ws_has_write_response)
			{
				return true;
			}
			if (!pump_peer_ws_messages(peer, 8, 50))
			{
				return false;
			}
			const int elapsed_ms = static_cast<int>(Clock::to_nanoseconds(Clock::now() - start).count() / 1000000LL);
			if (elapsed_ms >= timeout_ms)
			{
				return false;
			}
		}
	}
#endif

	bool TelemetryServer::Impl::resolve_ws_route(const char* uri, WsRoute& out_route) const
	{
		out_route = WsRoute{};
		if (!uri || !engine)
		{
			return false;
		}

		if (uri_equals(uri, "/api/telemetry/ws"))
		{
			out_route.valid = true;
			out_route.is_local = true;
			out_route.model_id = engine->get_model_name();
			return true;
		}

		if (!is_gateway)
		{
			return false;
		}

		constexpr const char* gateway_prefix = "/api/telemetry-gateway/";
		const char* rest = skip_prefix(uri, gateway_prefix);
		if (!rest || rest[0] == '\0' || !ends_with(uri, "/ws"))
		{
			return false;
		}

		const char* ws_suffix = ::strstr(rest, "/ws");
		if (!ws_suffix || ws_suffix == rest || ws_suffix[3] != '\0')
		{
			return false;
		}

		out_route.model_id.assign(rest, static_cast<size_t>(ws_suffix - rest));
		if (out_route.model_id.empty())
		{
			return false;
		}

		if (out_route.model_id == engine->get_model_name())
		{
			out_route.valid = true;
			out_route.is_local = true;
			return true;
		}

		const int peer_index = find_telemetry_peer_index_by_model_id(out_route.model_id.c_str());
		if (peer_index < 0)
		{
			return false;
		}

		out_route.valid = true;
		out_route.is_local = false;
		out_route.peer_index = peer_index;
		return true;
	}

	bool TelemetryServer::Impl::ws_connections_equal(const WebSocketConnection& lhs, const WebSocketConnection& rhs) const
	{
		if (lhs.conn && rhs.conn && lhs.conn == rhs.conn)
		{
			return true;
		}
		if (lhs.socket_fd >= 0 && rhs.socket_fd >= 0 && lhs.socket_fd == rhs.socket_fd)
		{
			if (!lhs.server || !rhs.server || lhs.server == rhs.server)
			{
				return true;
			}
		}
		return false;
	}

	int TelemetryServer::Impl::find_ws_client_index_unlocked(const WebSocketConnection& connection) const
	{
		for (size_t i = 0; i < ws_clients.size(); ++i)
		{
			const WsClient& client = ws_clients[i];
			if (!client.active)
			{
				continue;
			}
			if (ws_connections_equal(client.connection, connection))
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	bool TelemetryServer::Impl::register_ws_client(const WsRoute& route, const WebSocketConnection& incoming, int& out_index)
	{
		out_index = -1;
		if (!route.valid)
		{
			return false;
		}

		WebSocketConnection stored_connection = incoming;
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		if (stored_connection.server && stored_connection.socket_fd >= 0)
		{
			stored_connection.conn = nullptr;
		}
#endif

		LockGuard lock(ws_clients_mutex);
		const int existing_index = find_ws_client_index_unlocked(stored_connection);
		if (existing_index >= 0)
		{
			WsClient& existing = ws_clients[static_cast<size_t>(existing_index)];
			existing.active = true;
			existing.ready = false;
			existing.connection = stored_connection;
			existing.route = route;
			existing.last_heartbeat_at = Clock::now();
			out_index = existing_index;
			return true;
		}

		for (size_t i = 0; i < ws_clients.size(); ++i)
		{
			WsClient& slot = ws_clients[i];
			if (slot.active)
			{
				continue;
			}
			slot.active = true;
			slot.ready = false;
			slot.connection = stored_connection;
			slot.route = route;
			slot.last_heartbeat_at = Clock::now();
			out_index = static_cast<int>(i);
			return true;
		}

		return false;
	}

	void TelemetryServer::Impl::unregister_ws_client(const WebSocketConnection& connection)
	{
		LockGuard lock(ws_clients_mutex);
		const int index = find_ws_client_index_unlocked(connection);
		if (index < 0)
		{
			return;
		}
		ws_clients[static_cast<size_t>(index)] = WsClient{};
	}

	void TelemetryServer::Impl::send_ws_hello(const WsClient& client)
	{
		if (!client.active)
		{
			return;
		}

		FixedString1024 hello;
		hello.format("{\"type\":\"hello\",\"protocol_version\":1,\"model_id\":\"%s\",\"engine_session_id\":\"%s\",\"capabilities\":{"
					 "\"layout_text\":true,\"binary_frames\":true,\"write_ack\":true}}",
			client.route.model_id.c_str(),
			client.route.is_local ? session_id.c_str() : "");
		if (!client.connection.send_text(hello.c_str()))
		{
			unregister_ws_client(client.connection);
		}
	}

	void TelemetryServer::Impl::send_ws_error(const WebSocketConnection& connection, const char* error_text)
	{
		FixedString512 body;
		body.format("{\"type\":\"error\",\"error\":\"%s\"}", error_text ? error_text : "unknown");
		if (!connection.send_text(body.c_str()))
		{
			unregister_ws_client(connection);
		}
	}

	void TelemetryServer::Impl::send_ws_layout(const WsClient& client)
	{
		if (!client.active)
		{
			return;
		}

		if (client.route.is_local)
		{
			const bool refreshed = refresh_writable_input_connection_handles();
			last_known_writable_connection_handle_count = count_writable_input_connection_handles();
#if defined(ROBOTICK_PLATFORM_ESP32S3)
			if (refreshed)
			{
				rebuild_layout_cache();
			}
			LockGuard lock(layout_cache_mutex);
			if (!has_cached_layout_body)
			{
				send_ws_error(client.connection, "telemetry_layout_generation_failed");
				return;
			}
			if (!client.connection.send_frame(0x1, cached_layout_body, cached_layout_body_size))
			{
				unregister_ws_client(client.connection);
			}
#else
			(void)refreshed;
			json::StringSink layout_sink;
			json::Writer<json::StringSink> writer(layout_sink);
			write_layout_json_stream(writer, *engine, session_id.c_str(), writable_input_fields);
			writer.flush();
			if (!client.connection.send_frame(0x1, layout_sink.c_str(), layout_sink.size()))
			{
				unregister_ws_client(client.connection);
			}
#endif
			return;
		}

#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		if (client.route.peer_index < 0 || static_cast<size_t>(client.route.peer_index) >= telemetry_peers.size())
		{
			send_ws_error(client.connection, "telemetry_peer_not_found");
			return;
		}
		TelemetryPeerRoute& peer = telemetry_peers[static_cast<size_t>(client.route.peer_index)];
		LockGuard lock(peer.forwarded_request_mutex);
		if (!ensure_peer_ws_connection(peer, 100) || !wait_for_peer_layout(peer, 1000))
		{
			send_ws_error(client.connection, "telemetry_forward_failed");
			return;
		}
		if (!client.connection.send_frame(0x1,
				peer.peer_ws_latest_layout_size > 0 ? reinterpret_cast<const void*>(peer.peer_ws_latest_layout.data()) : "{}",
				peer.peer_ws_latest_layout_size))
		{
			unregister_ws_client(client.connection);
		}
#else
		send_ws_error(client.connection, "telemetry_forwarding_not_supported_on_this_platform");
#endif
	}

	void TelemetryServer::Impl::configure_websocket_endpoints()
	{
		web_server.clear_websocket_endpoints();

		WebSocketHandler handler;
		handler.on_connect = [this](const WebRequest& req, WebSocketConnection& connection)
		{
			WsRoute route;
			if (!resolve_ws_route(req.uri.c_str(), route))
			{
				return false;
			}
			int index = -1;
			if (!register_ws_client(route, connection, index))
			{
				return false;
			}
			(void)index;
			return true;
		};
		handler.on_ready = [this](WebSocketConnection& connection)
		{
			WsClient client;
			int client_index = -1;
			{
				LockGuard lock(ws_clients_mutex);
				client_index = find_ws_client_index_unlocked(connection);
				if (client_index < 0)
				{
					return;
				}
				client = ws_clients[static_cast<size_t>(client_index)];
			}
			send_ws_hello(client);
			send_ws_layout(client);
			{
				LockGuard lock(ws_clients_mutex);
				if (client_index < 0 || static_cast<size_t>(client_index) >= ws_clients.size())
				{
					return;
				}
				WsClient& live_client = ws_clients[static_cast<size_t>(client_index)];
				if (!live_client.active || !ws_connections_equal(live_client.connection, connection))
				{
					return;
				}
				live_client.ready = true;
			}
		};
		handler.on_message = [this](WebSocketConnection& connection, const WebSocketMessage& message)
		{
			WsRoute route;
			{
				LockGuard lock(ws_clients_mutex);
				const int index = find_ws_client_index_unlocked(connection);
				if (index < 0)
				{
					return false;
				}
				route = ws_clients[static_cast<size_t>(index)].route;
			}
			return handle_ws_message(route, connection, message);
		};
		handler.on_close = [this](const WebSocketConnection& connection)
		{
			unregister_ws_client(connection);
		};

		web_server.add_websocket_endpoint("/api/telemetry/ws", handler);
		if (!is_gateway || !engine)
		{
			return;
		}

		FixedString128 local_gateway_ws_uri;
		local_gateway_ws_uri.format("/api/telemetry-gateway/%s/ws", engine->get_model_name());
		web_server.add_websocket_endpoint(local_gateway_ws_uri.c_str(), handler);

		for (const TelemetryPeerRoute& peer : telemetry_peers)
		{
			FixedString128 peer_gateway_ws_uri;
			peer_gateway_ws_uri.format("/api/telemetry-gateway/%s/ws", peer.model_id.c_str());
			web_server.add_websocket_endpoint(peer_gateway_ws_uri.c_str(), handler);
		}
	}

	void TelemetryServer::Impl::start_ws_broadcast_loop()
	{
		ws_broadcast_stop.clear();
		if (ws_broadcast_thread.is_joining_supported() && ws_broadcast_thread.is_joinable())
		{
			ws_broadcast_thread.join();
		}
		ws_broadcast_thread = Thread(&TelemetryServer::Impl::ws_broadcast_entry, this, "TelemetryWsBroadcast");
	}

	void TelemetryServer::Impl::stop_ws_broadcast_loop()
	{
		ws_broadcast_stop.set();
		if (ws_broadcast_thread.is_joining_supported() && ws_broadcast_thread.is_joinable())
		{
			ws_broadcast_thread.join();
		}
	}

	void TelemetryServer::Impl::ws_broadcast_entry(void* user_data)
	{
		auto* self = static_cast<TelemetryServer::Impl*>(user_data);
		if (self)
		{
			self->ws_broadcast_loop();
		}
	}

	void TelemetryServer::Impl::ws_broadcast_loop()
	{
		while (!ws_broadcast_stop.is_set())
		{
			const Clock::time_point now = Clock::now();
			maybe_broadcast_local_layout_update();
			maybe_broadcast_local_frame(now);
			maybe_broadcast_peer_frames(now);
			maybe_send_heartbeats(now);
			Thread::sleep_ms(ws_broadcast_tick_ms);
		}
	}

	void TelemetryServer::Impl::maybe_broadcast_local_layout_update()
	{
		if (!engine)
		{
			return;
		}

		const bool refreshed = refresh_writable_input_connection_handles();
		const size_t current_count = count_writable_input_connection_handles();
		if (!refreshed && current_count == last_known_writable_connection_handle_count)
		{
			return;
		}

		last_known_writable_connection_handle_count = current_count;

#if defined(ROBOTICK_PLATFORM_ESP32S3)
		rebuild_layout_cache();
#endif

		FixedVector<WsClient, ws_max_clients> clients;
		{
			LockGuard lock(ws_clients_mutex);
			for (size_t i = 0; i < ws_clients.size(); ++i)
			{
				const WsClient& client = ws_clients[i];
				if (!client.active || !client.ready || !client.route.is_local)
				{
					continue;
				}
				clients.add(client);
			}
		}

		for (size_t i = 0; i < clients.size(); ++i)
		{
			send_ws_layout(clients[i]);
		}
	}

	void TelemetryServer::Impl::maybe_broadcast_local_frame(const Clock::time_point now)
	{
		(void)now;
		if (!engine)
		{
			return;
		}

		const WorkloadsBuffer& workloads_buffer = engine->get_workloads_buffer();
		const uint32_t frame_seq = workloads_buffer.get_telemetry_frame_seq();
		if ((frame_seq & 1u) != 0u || frame_seq == ws_last_local_frame_seq)
		{
			return;
		}

		FixedVector<WsClient, ws_max_clients> clients;
		{
			LockGuard lock(ws_clients_mutex);
			for (size_t i = 0; i < ws_clients.size(); ++i)
			{
				const WsClient& client = ws_clients[i];
				if (!client.active || !client.ready || !client.route.is_local)
				{
					continue;
				}
				clients.add(client);
			}
		}

		if (clients.empty())
		{
			ws_last_local_frame_seq = frame_seq;
			return;
		}

		const uint64_t timestamp_ns = static_cast<uint64_t>(Clock::to_nanoseconds(Clock::now().time_since_epoch()).count());
		FixedString512 metadata;
		metadata.format("{\"type\":\"frame\",\"engine_session_id\":\"%s\",\"frame_seq\":%u,\"timestamp_ns\":%llu,\"payload_size\":%u}",
			session_id.c_str(),
			static_cast<unsigned>(frame_seq),
			static_cast<unsigned long long>(timestamp_ns),
			static_cast<unsigned>(workloads_buffer.get_size_used()));

		for (size_t i = 0; i < clients.size(); ++i)
		{
			const WsClient& client = clients[i];
			const bool metadata_ok = client.connection.send_text(metadata.c_str());
			const bool frame_ok = metadata_ok && client.connection.send_binary(workloads_buffer.raw_ptr(), workloads_buffer.get_size_used());
			if (!frame_ok)
			{
				unregister_ws_client(client.connection);
			}
		}

		ws_last_local_frame_seq = frame_seq;
	}

	void TelemetryServer::Impl::maybe_broadcast_peer_frames(const Clock::time_point now)
	{
		(void)now;
#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		if (telemetry_peers.empty())
		{
			return;
		}

		for (size_t peer_index = 0; peer_index < telemetry_peers.size(); ++peer_index)
		{
			TelemetryPeerRoute& peer = telemetry_peers[peer_index];
			bool has_subscribers = false;
			FixedVector<WsClient, ws_max_clients> clients;
			{
				LockGuard lock(ws_clients_mutex);
				for (size_t i = 0; i < ws_clients.size(); ++i)
				{
					const WsClient& client = ws_clients[i];
					if (!client.active || !client.ready || client.route.is_local || client.route.peer_index != static_cast<int>(peer_index))
					{
						continue;
					}
					has_subscribers = true;
					clients.add(client);
				}
			}
			if (!has_subscribers)
			{
				continue;
			}

			{
				LockGuard lock(peer.forwarded_request_mutex);
				if (!ensure_peer_ws_connection(peer, 25))
				{
					continue;
				}
				if (!pump_peer_ws_messages(peer, 8, 0))
				{
					continue;
				}
				if (!peer.peer_ws_has_latest_raw)
				{
					continue;
				}
			}

			const uint32_t frame_seq = peer.peer_ws_latest_frame_seq;
			if ((frame_seq & 1u) != 0u)
			{
				continue;
			}
			if (!ws_last_peer_frame_seq.empty() && peer_index < ws_last_peer_frame_seq.size() && ws_last_peer_frame_seq[peer_index] == frame_seq)
			{
				continue;
			}

			const uint64_t timestamp_ns = static_cast<uint64_t>(Clock::to_nanoseconds(Clock::now().time_since_epoch()).count());
			FixedString512 metadata;
			metadata.format("{\"type\":\"frame\",\"engine_session_id\":\"%s\",\"frame_seq\":%u,\"timestamp_ns\":%llu,\"payload_size\":%u}",
				peer.peer_ws_latest_session_id.c_str(),
				static_cast<unsigned>(frame_seq),
				static_cast<unsigned long long>(timestamp_ns),
				static_cast<unsigned>(peer.peer_ws_latest_raw.size()));

			for (size_t i = 0; i < clients.size(); ++i)
			{
				const WsClient& client = clients[i];
				const bool metadata_ok = client.connection.send_text(metadata.c_str());
				const bool frame_ok =
					metadata_ok && client.connection.send_binary(
									   peer.peer_ws_latest_raw.size() > 0 ? reinterpret_cast<const void*>(peer.peer_ws_latest_raw.data()) : nullptr,
									   peer.peer_ws_latest_raw.size());
				if (!frame_ok)
				{
					unregister_ws_client(client.connection);
				}
			}

			if (!ws_last_peer_frame_seq.empty() && peer_index < ws_last_peer_frame_seq.size())
			{
				ws_last_peer_frame_seq[peer_index] = frame_seq;
			}
		}
#else
		(void)now;
#endif
	}

	void TelemetryServer::Impl::maybe_send_heartbeats(const Clock::time_point now)
	{
		FixedVector<WsClient, ws_max_clients> due_clients;
		{
			LockGuard lock(ws_clients_mutex);
			for (size_t i = 0; i < ws_clients.size(); ++i)
			{
				WsClient& client = ws_clients[i];
				if (!client.active || !client.ready)
				{
					continue;
				}
				const uint64_t elapsed_ns = static_cast<uint64_t>(Clock::to_nanoseconds(now - client.last_heartbeat_at).count());
				if (elapsed_ns < static_cast<uint64_t>(ws_heartbeat_interval_ms) * 1000000ULL)
				{
					continue;
				}
				client.last_heartbeat_at = now;
				due_clients.add(client);
			}
		}

		for (size_t i = 0; i < due_clients.size(); ++i)
		{
			const WsClient& client = due_clients[i];
			if (!client.connection.send_text("{\"type\":\"heartbeat\"}"))
			{
				unregister_ws_client(client.connection);
			}
		}
	}

	bool TelemetryServer::Impl::handle_ws_message(const WsRoute& route, const WebSocketConnection& connection, const WebSocketMessage& message)
	{
		if (message.opcode == 0x8)
		{
			return false;
		}
		if (message.opcode == 0x9)
		{
			connection.send_frame(0xA, message.data, message.size);
			return true;
		}
		if (message.opcode != 0x1 || !message.data || message.size == 0)
		{
			return true;
		}

		HeapVector<char> text;
		text.initialize(message.size + 1);
		::memcpy(text.data(), message.data, message.size);
		text[message.size] = '\0';

		if (!::strstr(text.data(), "\"writes\""))
		{
			return true;
		}

		if (route.is_local)
		{
			int status_code = WebResponseCode::InternalServerError;
			json::StringSink sink;
			build_write_response_json(text.data(), message.size, status_code, sink);
			if (!connection.send_text(sink.c_str()))
			{
				return false;
			}
			return true;
		}

#if defined(ROBOTICK_PLATFORM_LINUX) || defined(ROBOTICK_PLATFORM_DESKTOP)
		if (route.peer_index < 0 || static_cast<size_t>(route.peer_index) >= telemetry_peers.size())
		{
			send_ws_error(connection, "telemetry_peer_not_found");
			return true;
		}
		TelemetryPeerRoute& peer = telemetry_peers[static_cast<size_t>(route.peer_index)];
		LockGuard lock(peer.forwarded_request_mutex);
		if (!ensure_peer_ws_connection(peer, 100))
		{
			send_ws_error(connection, "telemetry_forward_failed");
			return true;
		}

		peer.peer_ws_has_write_response = false;
		if (!ws_send_masked_text(peer.peer_ws_fd, reinterpret_cast<const char*>(message.data), message.size))
		{
			close_peer_ws_connection(peer);
			send_ws_error(connection, "telemetry_forward_failed");
			return true;
		}

		if (!wait_for_peer_write_response(peer, 1200))
		{
			send_ws_error(connection, "telemetry_forward_failed");
			return true;
		}

		const bool sent = connection.send_frame(0x1,
			peer.peer_ws_write_response_size > 0 ? reinterpret_cast<const void*>(peer.peer_ws_write_response.data()) : "",
			peer.peer_ws_write_response_size);
		if (!sent)
		{
			return false;
		}
		return true;
#else
		(void)route;
		send_ws_error(connection, "telemetry_forwarding_not_supported_on_this_platform");
		return true;
#endif
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
			if (uri_equals(effective_uri, "/api/telemetry/workloads_buffer/layout"))
			{
				handle_get_workloads_buffer_layout(req, res);
				return true;
			}
			if (uri_equals(effective_uri, "/api/telemetry/workloads_buffer/raw"))
			{
				handle_get_workloads_buffer_raw(req, res);
				return true;
			}
		}
		else if (req.method.equals("POST"))
		{
			if (uri_equals(effective_uri, "/api/telemetry/set_workload_input_fields_data"))
			{
				handle_set_workload_input_fields_data(req, res);
				return true;
			}
			if (uri_equals(effective_uri, "/api/telemetry/set_workload_input_connection_state"))
			{
				handle_set_workload_input_connection_state(req, res);
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
		const bool is_connection_state_request = is_post && StringView(suffix_uri).equals("/set_workload_input_connection_state");

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

		if (is_write_request || is_connection_state_request)
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
						DataConnectionInputHandle* input_handle = engine->find_data_connection_input_handle_by_path(field_path.c_str());
						if (!input_handle)
						{
							input_handle = engine->find_data_connection_input_handle_by_overlap(field_ptr, field_type->size);
						}

						on_leaf_ref(field_path, field_type, field_ptr, input_handle);
					}
				};

				walk_struct(walk_struct, inputs_type, inputs_ptr, root_path, on_leaf);
			}
		};

		size_t writable_count = 0;
		for_each_writable_input_leaf(
			[&](const FixedString512&, const TypeDescriptor*, void*, DataConnectionInputHandle*)
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
			[&](const FixedString512& field_path, const TypeDescriptor* field_type, void* field_ptr, DataConnectionInputHandle* input_handle)
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
				writable.incoming_connection_handle = input_handle;
				pending_input_writes[write_index] = PendingInputWrite{};
				++write_index;
			});

		last_known_writable_connection_handle_count = count_writable_input_connection_handles();
	}

	size_t TelemetryServer::Impl::count_writable_input_connection_handles() const
	{
		size_t count = 0;
		for (size_t i = 0; i < writable_input_fields.size(); ++i)
		{
			if (writable_input_fields[i].incoming_connection_handle)
			{
				++count;
			}
		}
		return count;
	}

	bool TelemetryServer::Impl::refresh_writable_input_connection_handles()
	{
		if (!engine)
		{
			return false;
		}

		bool changed = false;
		for (size_t i = 0; i < writable_input_fields.size(); ++i)
		{
			WritableInputField& writable = writable_input_fields[i];
			if (writable.incoming_connection_handle)
			{
				continue;
			}

			DataConnectionInputHandle* input_handle = engine->find_data_connection_input_handle_by_path(writable.path.c_str());
			if (!input_handle && writable.target_ptr && writable.value_size > 0)
			{
				input_handle = engine->find_data_connection_input_handle_by_overlap(writable.target_ptr, writable.value_size);
			}
			if (writable.incoming_connection_handle != input_handle)
			{
				writable.incoming_connection_handle = input_handle;
				changed = true;
			}
		}

		return changed;
	}

	void TelemetryServer::Impl::build_write_response_json(
		const char* request_body, const size_t request_size, int& out_status_code, json::StringSink& out_json)
	{
		struct ParsedWrite
		{
			size_t writable_index = 0;
			uint16_t writable_handle = 0;
			PendingInputWrite staged;
		};

		auto write_error = [&](const int status, const char* error_text)
		{
			out_status_code = status;
			json::Writer<json::StringSink> writer(out_json);
			writer.start_object();
			writer.key("error");
			writer.string(error_text ? error_text : "unknown_error");
			writer.end_object();
			writer.flush();
		};

		if (!engine)
		{
			write_error(WebResponseCode::ServiceUnavailable, "engine_not_available");
			return;
		}

		TelemetryWriteRequestPayload payload;
		JsonCursor cursor(request_body, request_size);
		if (!cursor.parse_write_request(payload))
		{
			write_error(WebResponseCode::BadRequest, "invalid_json");
			return;
		}

		const bool session_matches = !payload.has_engine_session_id || StringView(session_id.c_str()).equals(payload.engine_session_id.c_str());
		if (!payload.has_writes || payload.write_count == 0)
		{
			write_error(WebResponseCode::BadRequest, "missing_writes");
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
			write_error(parse_failed_status, parse_failed_error);
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

		out_status_code = WebResponseCode::OK;
		json::Writer<json::StringSink> writer(out_json);
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
		writer.flush();
	}

	void TelemetryServer::Impl::build_connection_state_response_json(
		const char* request_body, const size_t request_size, int& out_status_code, json::StringSink& out_json)
	{
		struct UpdateResult
		{
			uint16_t field_handle = 0;
			FixedString512 field_path;
			uint16_t connection_handle = 0;
			bool enabled = true;
			const char* status = "";
		};

		auto write_error = [&](const int status, const char* error_text)
		{
			out_status_code = status;
			json::Writer<json::StringSink> writer(out_json);
			writer.start_object();
			writer.key("error");
			writer.string(error_text ? error_text : "unknown_error");
			writer.end_object();
			writer.flush();
		};

		if (!engine)
		{
			write_error(WebResponseCode::ServiceUnavailable, "engine_not_available");
			return;
		}

		refresh_writable_input_connection_handles();

		TelemetryConnectionStateRequestPayload payload;
		JsonCursor cursor(request_body, request_size);
		if (!cursor.parse_connection_state_request(payload))
		{
			write_error(WebResponseCode::BadRequest, "invalid_json");
			return;
		}

		const bool session_matches = !payload.has_engine_session_id || StringView(session_id.c_str()).equals(payload.engine_session_id.c_str());
		if (!payload.has_updates || payload.update_count == 0)
		{
			write_error(WebResponseCode::BadRequest, "missing_updates");
			return;
		}

		HeapVector<UpdateResult> results;
		results.initialize(payload.update_count);

		for (size_t update_payload_index = 0; update_payload_index < payload.update_count; ++update_payload_index)
		{
			const TelemetryConnectionStateRequestEntry& update_payload = payload.updates[update_payload_index];
			if (!session_matches && !update_payload.has_field_path)
			{
				write_error(WebResponseCode::PreconditionFailed, "field_path_required_for_stale_session_write");
				return;
			}

			int writable_index = -1;
			uint16_t writable_handle = 0;
			FixedString512 requested_path;

			if (update_payload.has_field_handle)
			{
				writable_handle = update_payload.field_handle;
				writable_index = find_writable_input_index_by_handle(writable_handle);
			}

			if (update_payload.has_field_path)
			{
				requested_path = update_payload.field_path.c_str();
				const int path_index = find_writable_input_index_by_path(requested_path.c_str());
				if (path_index >= 0)
				{
					if (writable_index >= 0 && static_cast<size_t>(writable_index) != static_cast<size_t>(path_index))
					{
						write_error(WebResponseCode::BadRequest, "field_handle_path_mismatch");
						return;
					}
					writable_index = path_index;
					writable_handle = writable_input_fields[static_cast<size_t>(writable_index)].handle;
				}
			}

			if (writable_index < 0)
			{
				write_error(WebResponseCode::NotFound, "writable_input_not_found");
				return;
			}

			if (!update_payload.has_enabled)
			{
				write_error(WebResponseCode::BadRequest, "missing_enabled");
				return;
			}

			WritableInputField& writable = writable_input_fields[static_cast<size_t>(writable_index)];
			if (!writable.incoming_connection_handle)
			{
				write_error(WebResponseCode::Conflict, "writable_input_has_no_incoming_connection");
				return;
			}

			writable.incoming_connection_handle->set_enabled(update_payload.enabled);

			UpdateResult& result = results[update_payload_index];
			result.field_handle = writable_handle;
			result.field_path = writable.path.c_str();
			result.connection_handle = writable.incoming_connection_handle->handle_id;
			result.enabled = writable.incoming_connection_handle->is_enabled();
			result.status = "accepted";
		}

#if defined(ROBOTICK_PLATFORM_ESP32S3)
		rebuild_layout_cache();
#endif

		out_status_code = WebResponseCode::OK;
		json::Writer<json::StringSink> writer(out_json);
		writer.start_object();
		writer.key("status");
		writer.string("processed");
		writer.key("engine_session_id");
		writer.string(session_id.c_str());
		writer.key("updates");
		writer.start_array();
		for (size_t i = 0; i < payload.update_count; ++i)
		{
			const UpdateResult& result = results[i];
			writer.start_object();
			writer.key("field_handle");
			writer.uint64(result.field_handle);
			writer.key("field_path");
			writer.string(result.field_path);
			writer.key("incoming_connection_handle");
			writer.uint64(result.connection_handle);
			writer.key("incoming_connection_enabled");
			writer.boolean(result.enabled);
			writer.key("status");
			writer.string(result.status);
			writer.end_object();
		}
		writer.end_array();
		writer.end_object();
		writer.flush();
	}

	void TelemetryServer::Impl::handle_set_workload_input_fields_data(const WebRequest& req, WebResponse& res)
	{
		json::StringSink response_body;
		int status_code = WebResponseCode::InternalServerError;
		build_write_response_json(reinterpret_cast<const char*>(req.body.data()), req.body.size(), status_code, response_body);

		res.set_status_code(status_code);
		res.set_content_type("application/json");
		res.set_body(response_body.c_str(), response_body.size());
	}

	void TelemetryServer::Impl::handle_set_workload_input_connection_state(const WebRequest& req, WebResponse& res)
	{
		json::StringSink response_body;
		int status_code = WebResponseCode::InternalServerError;
		build_connection_state_response_json(reinterpret_cast<const char*>(req.body.data()), req.body.size(), status_code, response_body);

		res.set_status_code(status_code);
		res.set_content_type("application/json");
		res.set_body(response_body.c_str(), response_body.size());
	}

	static FixedString64 make_dynamic_struct_type_name(const TypeDescriptor& type_desc, const DynamicStructDescriptor& desc, void* data_ptr)
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
		const char* base_name = type_desc.name.empty() ? "DynamicStruct" : type_desc.name.c_str();
		type_name.format("%s_%08X", base_name, static_cast<unsigned int>(hash.final()));
		return type_name;
	}

	static FixedString256 get_type_name(const TypeDescriptor& type_desc, void* data_ptr)
	{
		const DynamicStructDescriptor* dynamic_struct_desc = type_desc.get_dynamic_struct_desc();
		if (dynamic_struct_desc)
		{
			const FixedString64 blackboard_name = make_dynamic_struct_type_name(type_desc, *dynamic_struct_desc, data_ptr);
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
			if (writable.incoming_connection_handle)
			{
				writer.key("incoming_connection_handle");
				writer.uint64(static_cast<uint64_t>(writable.incoming_connection_handle->handle_id));
				writer.key("incoming_connection_path");
				writer.string(writable.incoming_connection_handle->path.c_str());
				writer.key("incoming_connection_enabled");
				writer.boolean(writable.incoming_connection_handle->is_enabled());
			}
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
		if (refresh_writable_input_connection_handles())
		{
			last_known_writable_connection_handle_count = count_writable_input_connection_handles();
			rebuild_layout_cache();
		}
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
		refresh_writable_input_connection_handles();
		last_known_writable_connection_handle_count = count_writable_input_connection_handles();
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
