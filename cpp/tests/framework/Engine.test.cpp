// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/config/AssertUtils.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/containers/DynamicStructStorageVector.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/data/TelemetryServer.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/json/Json.h"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <curl/curl.h>
#include <cstdio>
#include <filesystem>
#include <netinet/in.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace robotick::test
{
	namespace
	{
		struct HttpTextBuffer
		{
			char data[16384] = {};
			size_t len = 0;

			void append(const char* src, size_t count)
			{
				if (!src || count == 0)
					return;
				const size_t cap = sizeof(data) - 1;
				const size_t room = len < cap ? cap - len : 0;
				const size_t to_copy = room < count ? room : count;
				if (to_copy > 0)
				{
					::memcpy(data + len, src, to_copy);
					len += to_copy;
					data[len] = '\0';
				}
			}
		};

		struct HttpResponse
		{
			long status_code = 0;
			HttpTextBuffer body;
			HttpTextBuffer session_id;
		};

		size_t curl_write_to_text(char* ptr, size_t size, size_t nmemb, void* userdata)
		{
			auto& out = *static_cast<HttpTextBuffer*>(userdata);
			const size_t bytes = size * nmemb;
			out.append(ptr, bytes);
			return bytes;
		}

		size_t curl_capture_headers(char* ptr, size_t size, size_t nmemb, void* userdata)
		{
			auto& response = *static_cast<HttpResponse*>(userdata);
			const size_t bytes = size * nmemb;
			const char* prefix = "X-Robotick-Session-Id:";
			if (bytes > ::strlen(prefix) && ::strncasecmp(ptr, prefix, ::strlen(prefix)) == 0)
			{
				const char* value = ptr + ::strlen(prefix);
				while (*value == ' ' || *value == '\t')
					++value;
				const char* end = value + bytes;
				while (end > value && (end[-1] == '\r' || end[-1] == '\n'))
					--end;
				response.session_id.append(value, static_cast<size_t>(end - value));
			}
			return bytes;
		}

		bool contains_text(const char* haystack, const char* needle)
		{
			return haystack && needle && ::strstr(haystack, needle) != nullptr;
		}

		const char* resolve_layout_schema_path()
		{
			static std::string resolved_path;
			if (!resolved_path.empty())
				return resolved_path.c_str();

			namespace fs = std::filesystem;
			const fs::path this_file = fs::path(__FILE__);
			const fs::path repo_root = this_file.parent_path().parent_path().parent_path().parent_path();
			const fs::path schema_path = repo_root / "schemas" / "workloads_layout.schema.json";

			if (fs::exists(schema_path) && fs::is_regular_file(schema_path))
			{
				resolved_path = schema_path.string();
				return resolved_path.c_str();
			}
			return nullptr;
		}

		bool validate_json_against_schema(const char* schema_path, const char* instance_json, char* out_error, const size_t out_error_size)
		{
			if (!schema_path || !instance_json || !out_error || out_error_size == 0)
			{
				return false;
			}
			out_error[0] = '\0';

			FILE* schema_file = ::fopen(schema_path, "rb");
			if (!schema_file)
			{
				::snprintf(out_error, out_error_size, "cannot open schema file");
				return false;
			}

			char schema_read_buffer[65536];
			rapidjson::FileReadStream schema_stream(schema_file, schema_read_buffer, sizeof(schema_read_buffer));
			rapidjson::Document schema_doc;
			schema_doc.ParseStream(schema_stream);
			::fclose(schema_file);
			if (schema_doc.HasParseError())
			{
				::snprintf(out_error,
					out_error_size,
					"schema parse error: %s at offset %llu",
					rapidjson::GetParseError_En(schema_doc.GetParseError()),
					static_cast<unsigned long long>(schema_doc.GetErrorOffset()));
				return false;
			}

			rapidjson::SchemaDocument schema(schema_doc);
			if (!schema.GetError().ObjectEmpty())
			{
				::snprintf(out_error, out_error_size, "schema compile error");
				return false;
			}

			rapidjson::Document instance_doc;
			instance_doc.Parse(instance_json);
			if (instance_doc.HasParseError())
			{
				::snprintf(out_error,
					out_error_size,
					"instance parse error: %s at offset %llu",
					rapidjson::GetParseError_En(instance_doc.GetParseError()),
					static_cast<unsigned long long>(instance_doc.GetErrorOffset()));
				return false;
			}

			rapidjson::SchemaValidator validator(schema);
			if (!instance_doc.Accept(validator))
			{
				rapidjson::StringBuffer sb_instance;
				validator.GetInvalidDocumentPointer().StringifyUriFragment(sb_instance);
				rapidjson::StringBuffer sb_schema;
				validator.GetInvalidSchemaPointer().StringifyUriFragment(sb_schema);
				::snprintf(out_error,
					out_error_size,
					"schema validation failed: instance=%s schema=%s keyword=%s",
					sb_instance.GetString(),
					sb_schema.GetString(),
					validator.GetInvalidSchemaKeyword());
				return false;
			}

			return true;
		}

		HttpResponse http_request(const char* url, const char* method = "GET", const char* body = nullptr)
		{
			CURL* curl = curl_easy_init();
			REQUIRE(curl != nullptr);

			HttpResponse response;
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_text);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_capture_headers);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
			if (::strcmp(method, "POST") == 0)
			{
				curl_easy_setopt(curl, CURLOPT_POST, 1L);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body ? static_cast<long>(::strlen(body)) : 0L);
				struct curl_slist* headers = nullptr;
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
				curl_easy_perform(curl);
				curl_slist_free_all(headers);
			}
			else
			{
				curl_easy_perform(curl);
			}

			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
			curl_easy_cleanup(curl);
			return response;
		}

		struct EngineRunContext
		{
			Engine* engine = nullptr;
			AtomicFlag* stop_flag = nullptr;
		};

		class EngineRunThread
		{
		  public:
			EngineRunThread(Engine& engine, AtomicFlag& stop_flag)
			{
				context.engine = &engine;
				context.stop_flag = &stop_flag;
				thread = Thread(&EngineRunThread::run_entry, &context, "EngineRunThread");
			}

			~EngineRunThread()
			{
				if (thread.is_joining_supported() && thread.is_joinable())
				{
					thread.join();
				}
			}

			EngineRunThread(const EngineRunThread&) = delete;
			EngineRunThread& operator=(const EngineRunThread&) = delete;
			EngineRunThread(EngineRunThread&&) = delete;
			EngineRunThread& operator=(EngineRunThread&&) = delete;

		  private:
			static void run_entry(void* user_data)
			{
				auto* ctx = static_cast<EngineRunContext*>(user_data);
				if (ctx->engine && ctx->stop_flag)
				{
					ctx->engine->run(*ctx->stop_flag);
				}
			}

			EngineRunContext context{};
			Thread thread;
		};

		struct WsFrame
		{
			uint8_t opcode = 0;
			HeapVector<uint8_t> payload;
			size_t payload_size = 0;

			void clear()
			{
				opcode = 0;
				payload.reset();
				payload_size = 0;
			}
		};

		class WsTestClient
		{
		  public:
			~WsTestClient() { disconnect(); }

			bool connect(const char* host, uint16_t port, const char* path)
			{
				if (!host || !path)
				{
					return false;
				}

				sock = ::socket(AF_INET, SOCK_STREAM, 0);
				if (sock < 0)
				{
					return false;
				}

				timeval timeout{};
				timeout.tv_sec = 0;
				timeout.tv_usec = 200000;
				::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
				::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

				sockaddr_in addr{};
				addr.sin_family = AF_INET;
				addr.sin_port = htons(port);
				if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1)
				{
					disconnect();
					return false;
				}
				if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
				{
					disconnect();
					return false;
				}

				char request[1024];
				::snprintf(request,
					sizeof(request),
					"GET %s HTTP/1.1\r\n"
					"Host: %s:%u\r\n"
					"Upgrade: websocket\r\n"
					"Connection: Upgrade\r\n"
					"Sec-WebSocket-Version: 13\r\n"
					"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
					"\r\n",
					path,
					host,
					static_cast<unsigned>(port));
				if (!send_all(reinterpret_cast<const uint8_t*>(request), ::strlen(request)))
				{
					disconnect();
					return false;
				}

				char response[2048] = {};
				size_t response_len = 0;
				while (response_len + 1 < sizeof(response))
				{
					char ch = '\0';
					const ssize_t n = ::recv(sock, &ch, 1, 0);
					if (n <= 0)
					{
						disconnect();
						return false;
					}
					response[response_len++] = ch;
					response[response_len] = '\0';
					if (::strstr(response, "\r\n\r\n") != nullptr)
					{
						break;
					}
				}

				if (::strstr(response, " 101 ") == nullptr || ::strstr(response, "Sec-WebSocket-Accept:") == nullptr)
				{
					disconnect();
					return false;
				}
				return true;
			}

			void disconnect()
			{
				if (sock >= 0)
				{
					::close(sock);
					sock = -1;
				}
			}

			bool send_text(const char* text)
			{
				const char* payload_text = text ? text : "";
				const size_t payload_len = ::strlen(payload_text);
				if (payload_len > 0xFFFF)
					return false;
				const size_t extended_len_bytes = payload_len <= 125 ? 0 : 2;
				const size_t frame_size = 2 + extended_len_bytes + 4 + payload_len;

				HeapVector<uint8_t> frame;
				frame.initialize(frame_size);
				size_t at = 0;
				frame[at++] = static_cast<uint8_t>(0x80 | 0x1);
				if (payload_len <= 125)
				{
					frame[at++] = static_cast<uint8_t>(0x80 | payload_len);
				}
				else
				{
					frame[at++] = static_cast<uint8_t>(0x80 | 126);
					frame[at++] = static_cast<uint8_t>((payload_len >> 8) & 0xFF);
					frame[at++] = static_cast<uint8_t>(payload_len & 0xFF);
				}

				const uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
				frame[at++] = mask[0];
				frame[at++] = mask[1];
				frame[at++] = mask[2];
				frame[at++] = mask[3];
				for (size_t i = 0; i < payload_len; ++i)
				{
					frame[at++] = static_cast<uint8_t>(payload_text[i]) ^ mask[i % 4];
				}
				return send_all(frame.data(), at);
			}

			bool send_pong(const uint8_t* payload, size_t payload_len)
			{
				if (payload_len > 125)
				{
					return false;
				}

				HeapVector<uint8_t> frame;
				frame.initialize(payload_len + 6);
				size_t at = 0;
				frame[at++] = static_cast<uint8_t>(0x80 | 0xA);
				frame[at++] = static_cast<uint8_t>(0x80 | payload_len);
				const uint8_t mask[4] = {0x21, 0x43, 0x65, 0x87};
				frame[at++] = mask[0];
				frame[at++] = mask[1];
				frame[at++] = mask[2];
				frame[at++] = mask[3];
				for (size_t i = 0; i < payload_len; ++i)
				{
					const uint8_t value = payload ? payload[i] : 0;
					frame[at++] = value ^ mask[i % 4];
				}
				return send_all(frame.data(), at);
			}

			bool receive_frame(WsFrame& out_frame)
			{
				out_frame.clear();
				uint8_t header[2] = {};
				if (!recv_exact(header, sizeof(header)))
				{
					return false;
				}

				out_frame.opcode = static_cast<uint8_t>(header[0] & 0x0F);
				const bool masked = (header[1] & 0x80) != 0;
				uint64_t payload_len = static_cast<uint64_t>(header[1] & 0x7F);

				if (payload_len == 126)
				{
					uint8_t ext[2] = {};
					if (!recv_exact(ext, sizeof(ext)))
					{
						return false;
					}
					payload_len = (static_cast<uint64_t>(ext[0]) << 8) | static_cast<uint64_t>(ext[1]);
				}
				else if (payload_len == 127)
				{
					uint8_t ext[8] = {};
					if (!recv_exact(ext, sizeof(ext)))
					{
						return false;
					}
					payload_len = 0;
					for (size_t i = 0; i < sizeof(ext); ++i)
					{
						payload_len = (payload_len << 8) | static_cast<uint64_t>(ext[i]);
					}
				}

				if (payload_len > (8U * 1024U * 1024U))
				{
					return false;
				}

				uint8_t mask[4] = {};
				if (masked)
				{
					if (!recv_exact(mask, sizeof(mask)))
					{
						return false;
					}
				}

				out_frame.payload_size = static_cast<size_t>(payload_len);
				if (out_frame.payload_size > 0)
				{
					out_frame.payload.initialize(out_frame.payload_size);
					if (!recv_exact(out_frame.payload.data(), out_frame.payload_size))
					{
						return false;
					}
				}

				if (masked)
				{
					for (size_t i = 0; i < out_frame.payload_size; ++i)
					{
						out_frame.payload[i] ^= mask[i % 4];
					}
				}
				return true;
			}

		  private:
			bool send_all(const uint8_t* data, size_t size)
			{
				size_t sent = 0;
				while (sent < size)
				{
					const ssize_t n = ::send(sock, data + sent, size - sent, 0);
					if (n <= 0)
					{
						return false;
					}
					sent += static_cast<size_t>(n);
				}
				return true;
			}

			bool recv_exact(void* data, size_t size)
			{
				uint8_t* out = static_cast<uint8_t*>(data);
				size_t received = 0;
				while (received < size)
				{
					const ssize_t n = ::recv(sock, out + received, size - received, 0);
					if (n <= 0)
					{
						return false;
					}
					received += static_cast<size_t>(n);
				}
				return true;
			}

			int sock = -1;
		};

		bool read_ws_application_frame(WsTestClient& client, WsFrame& out_frame, int max_attempts = 16)
		{
			for (int attempt = 0; attempt < max_attempts; ++attempt)
			{
				if (!client.receive_frame(out_frame))
				{
					return false;
				}
				if (out_frame.opcode == 0x9)
				{
					if (!client.send_pong(out_frame.payload_size > 0 ? out_frame.payload.data() : nullptr, out_frame.payload_size))
					{
						return false;
					}
					continue;
				}
				if (out_frame.opcode == 0xA)
				{
					continue;
				}
				if (out_frame.opcode == 0x8)
				{
					return false;
				}
				return true;
			}
			return false;
		}

		bool frame_to_text(const WsFrame& frame, HeapVector<char>& out_text)
		{
			if (frame.opcode != 0x1)
			{
				return false;
			}
			out_text.reset();
			out_text.initialize(frame.payload_size + 1);
			if (frame.payload_size > 0)
			{
				::memcpy(out_text.data(), frame.payload.data(), frame.payload_size);
			}
			out_text[frame.payload_size] = '\0';
			return true;
		}

		bool wait_for_ws_text_message(WsTestClient& client, HeapVector<char>& out_text, int max_frames = 32)
		{
			WsFrame frame;
			for (int i = 0; i < max_frames; ++i)
			{
				if (!read_ws_application_frame(client, frame))
				{
					return false;
				}
				if (frame_to_text(frame, out_text))
				{
					return true;
				}
			}
			return false;
		}

		bool wait_for_ws_frame_metadata_and_payload(WsTestClient& client, json::Document& metadata_doc, size_t& payload_size, int max_frames = 48)
		{
			WsFrame frame;
			HeapVector<char> text;
			bool saw_metadata = false;
			size_t expected_payload_size = 0;
			payload_size = 0;

			for (int i = 0; i < max_frames; ++i)
			{
				if (!read_ws_application_frame(client, frame))
				{
					return false;
				}

				if (!saw_metadata)
				{
					if (frame.opcode != 0x1)
					{
						continue;
					}
					if (!frame_to_text(frame, text))
					{
						continue;
					}
					if (!metadata_doc.parse(text.data()))
					{
						continue;
					}
					const json::Value root = metadata_doc.root();
					if (!root.is_object() || !root.contains("type") || !root["type"].equals("frame"))
					{
						continue;
					}
					expected_payload_size = static_cast<size_t>(root["payload_size"].get_uint64());
					saw_metadata = true;
					continue;
				}

				if (frame.opcode == 0x2)
				{
					payload_size = frame.payload_size;
					return payload_size == expected_payload_size;
				}
			}

			return false;
		}
	} // namespace
	struct TestSequencedGroupWorkload
	{
	};

	ROBOTICK_REGISTER_WORKLOAD(TestSequencedGroupWorkload)

	struct OverrunWorkload
	{
		void tick(const TickInfo& tick_info)
		{
			(void)tick_info;
			Thread::sleep_ms(10);
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(OverrunWorkload)

	namespace
	{
		// === DummyWorkload (with config/inputs/load) ===

		struct DummyConfig
		{
			int value = 0;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyConfig)
		ROBOTICK_STRUCT_FIELD(DummyConfig, int, value)
		ROBOTICK_REGISTER_STRUCT_END(DummyConfig)

		struct DummyInputs
		{
			float input_float = 0.f;
			FixedString64 input_string_64;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyInputs)
		ROBOTICK_STRUCT_FIELD(DummyInputs, float, input_float)
		ROBOTICK_STRUCT_FIELD(DummyInputs, FixedString64, input_string_64)
		ROBOTICK_REGISTER_STRUCT_END(DummyInputs)

		struct DummyWorkload
		{
			DummyConfig config;
			DummyInputs inputs;
		};
		ROBOTICK_REGISTER_WORKLOAD(DummyWorkload, DummyConfig, DummyInputs)

		// === TickCounterWorkload ===

		struct TickCounterWorkload
		{
			int count = 0;
			void tick(const TickInfo&) { count++; }
		};
		ROBOTICK_REGISTER_WORKLOAD(TickCounterWorkload)

		struct ThreadAffinityWorkload
		{
			Thread::ThreadId start_thread = 0;
			Thread::ThreadId first_tick_thread = 0;
			AtomicValue<int> tick_count{0};

			void start(float) { start_thread = Thread::get_current_thread_id(); }

			void tick(const TickInfo&)
			{
				const int previous = tick_count.fetch_add(1);
				if (previous == 0)
				{
					first_tick_thread = Thread::get_current_thread_id();
				}
			}
		};
		ROBOTICK_REGISTER_WORKLOAD(ThreadAffinityWorkload)

		enum class LayoutTestEnum : uint32_t
		{
			Alpha = 0,
			Beta = 1,
			Gamma = 2
		};
		ROBOTICK_REGISTER_ENUM_BEGIN(LayoutTestEnum)
		ROBOTICK_ENUM_VALUE("Alpha", LayoutTestEnum::Alpha)
		ROBOTICK_ENUM_VALUE("Beta", LayoutTestEnum::Beta)
		ROBOTICK_ENUM_VALUE("Gamma", LayoutTestEnum::Gamma)
		ROBOTICK_REGISTER_ENUM_END(LayoutTestEnum)

		struct LayoutEnumConfig
		{
			LayoutTestEnum status = LayoutTestEnum::Alpha;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(LayoutEnumConfig)
		ROBOTICK_STRUCT_FIELD(LayoutEnumConfig, LayoutTestEnum, status)
		ROBOTICK_REGISTER_STRUCT_END(LayoutEnumConfig)

		struct LayoutEnumWorkload
		{
			LayoutEnumConfig config;
		};
		ROBOTICK_REGISTER_WORKLOAD(LayoutEnumWorkload, LayoutEnumConfig)

		using TestDynamicByteBuffer = DynamicStructStorageVector<uint8_t, GET_TYPE_ID(uint8_t).value>;
		ROBOTICK_REGISTER_DYNAMIC_STRUCT(TestDynamicByteBuffer,
			TestDynamicByteBuffer::resolve_descriptor,
			TestDynamicByteBuffer::plan_storage,
			TestDynamicByteBuffer::bind_storage)

		struct LargeDynamicConfig
		{
			uint32_t max_output_bytes = 0;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(LargeDynamicConfig)
		ROBOTICK_STRUCT_FIELD(LargeDynamicConfig, uint32_t, max_output_bytes)
		ROBOTICK_REGISTER_STRUCT_END(LargeDynamicConfig)

		struct LargeDynamicOutputs
		{
			TestDynamicByteBuffer bytes;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(LargeDynamicOutputs)
		ROBOTICK_STRUCT_FIELD(LargeDynamicOutputs, TestDynamicByteBuffer, bytes)
		ROBOTICK_REGISTER_STRUCT_END(LargeDynamicOutputs)

		struct LargeDynamicWorkload
		{
			LargeDynamicConfig config;
			LargeDynamicOutputs outputs;

			void pre_load() { outputs.bytes.initialize_capacity(config.max_output_bytes); }
		};
		ROBOTICK_REGISTER_WORKLOAD(LargeDynamicWorkload, LargeDynamicConfig, void, LargeDynamicOutputs)

	} // namespace

	// === Utility helpers ===

	uint16_t find_free_port_for_test()
	{
		int sock = ::socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return 0;

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = 0;

		if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
		{
			::close(sock);
			return 0;
		}

		socklen_t addr_len = sizeof(addr);
		if (::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0)
		{
			::close(sock);
			return 0;
		}

		const uint16_t port = ntohs(addr.sin_port);
		::close(sock);
		// NOTE: this is still vulnerable to a TOCTOU race—the port is released before the Engine binds, so another
		// process could grab it. We accept occasional flake on busy hosts, or keep extending the search loop if needed.
		return port;
	}

	uint16_t choose_telemetry_port()
	{
		for (int attempt = 0; attempt < 3; ++attempt)
		{
			const uint16_t port = find_free_port_for_test();
			if (port != 0)
				return port;
		}
		ROBOTICK_FATAL_EXIT("Engine test: no free telemetry port found after 3 attempts");
		return 0;
	}

	bool bind_to_port(uint16_t port)
	{
		int sock = ::socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return false;

		int reuse = 1;
		::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(port);

		bool success = (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
		::close(sock);
		return success;
	}

	// === Tests ===

	TEST_CASE("Unit/Framework/Engine")
	{
		SECTION("DummyWorkload stores tick rate correctly")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("DummyWorkload"),
				StringView("A"),
				123.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			float tick_rate = engine.get_root_instance_info()->seed->tick_rate_hz;
			REQUIRE(tick_rate == Catch::Approx(123.0f));
		}

		SECTION("DummyWorkload config is loaded via load()")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry inputs[] = {{"input_string_64", "hello there"}, {"input_float", "1.234"}};
			static const WorkloadSeed workload_seed{
				TypeId("DummyWorkload"),
				StringView("A"),
				1.0f,
				{},	   // children
				{},	   // config
				inputs // inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->inputs.input_float == 1.234f);
			REQUIRE(ptr->inputs.input_string_64 == "hello there");
		}

		SECTION("DummyWorkload inputs are loaded via load()")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry config[] = {{"value", "42"}};
			static const WorkloadSeed workload_seed{
				TypeId("DummyWorkload"),
				StringView("A"),
				1.0f,
				{}, // children
				config,
				{} // inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->config.value == 42);
		}

		SECTION("Multiple workloads supported")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry config_one[] = {{"value", "1"}};
			static const FieldConfigEntry config_two[] = {{"value", "2"}};
			static const WorkloadSeed workload_one{
				TypeId("DummyWorkload"),
				StringView("one"),
				1.0f,
				{},			// children
				config_one, // config
				{}			// inputs
			};
			static const WorkloadSeed workload_two{
				TypeId("DummyWorkload"),
				StringView("two"),
				1.0f,
				{},			// children
				config_two, // config
				{}			// inputs
			};
			static const WorkloadSeed* const root_children[] = {&workload_one, &workload_two};
			static const WorkloadSeed root{
				TypeId("TestSequencedGroupWorkload"),
				StringView("group"),
				1.0f,
				root_children,
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_one, &workload_two, &root};
			model.use_workload_seeds(workloads);
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			const DummyWorkload* one = engine.find_instance<DummyWorkload>("one");
			const DummyWorkload* two = engine.find_instance<DummyWorkload>("two");

			REQUIRE(one->config.value == 1);
			REQUIRE(two->config.value == 2);
		}

		SECTION("Workloads receive tick call")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("TickCounterWorkload"),
				StringView("ticky"),
				200.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_after_next_tick_flag{true};
			engine.run(stop_after_next_tick_flag); // will tick at least once even if stop_after_next_tick_flag is true

			const TickCounterWorkload* ptr = engine.find_instance<TickCounterWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->count >= 1);
		}

		SECTION("Overrun workload increments counter")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("OverrunWorkload"),
				StringView("overrun"),
				1000.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_after_next_tick_flag{false};
			EngineRunThread runner(engine, stop_after_next_tick_flag);

			Thread::sleep_ms(50);
			stop_after_next_tick_flag.set();

			const WorkloadInstanceInfo* root_info = engine.get_root_instance_info();
			REQUIRE(root_info != nullptr);
			REQUIRE(root_info->workload_stats != nullptr);
			CHECK(root_info->workload_stats->overrun_count > 0);
		}

		SECTION("Telemetry + remote lifecycle")
		{
			const uint16_t telemetry_port = choose_telemetry_port();
			REQUIRE(telemetry_port != 0);

			auto run_engine_once = [&](uint16_t port)
			{
				Model model;
				model.set_telemetry_port(port);
				static const WorkloadSeed workload_seed{
					TypeId("TickCounterWorkload"),
					StringView("telemetry_ticky"),
					200.0f,
					{}, // children
					{}, // config
					{}	// inputs
				};
				static const WorkloadSeed* const workloads[] = {&workload_seed};
				model.use_workload_seeds(workloads);
				model.set_root_workload(workload_seed);

				Engine engine;
				engine.load(model);

				AtomicFlag stop_flag{false};
				EngineRunThread runner(engine, stop_flag);

				Thread::sleep_ms(30);
				stop_flag.set();
			};

			run_engine_once(telemetry_port);
			REQUIRE(bind_to_port(telemetry_port));
			run_engine_once(telemetry_port);
		}

		SECTION("Telemetry websocket layout emits enum metadata")
		{
			Model model;
			const uint16_t telemetry_port = choose_telemetry_port();
			model.set_telemetry_port(telemetry_port);
			static const WorkloadSeed layout_workload_seed{TypeId("LayoutEnumWorkload"), StringView("layout_enum"), 30.0f, {}, {}, {}};
			static const WorkloadSeed root_workload_seed{TypeId("TickCounterWorkload"), StringView("layout_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const workloads[] = {&layout_workload_seed, &root_workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(root_workload_seed);

			Engine engine;
			engine.load(model);
			AtomicFlag stop_flag{false};
			EngineRunThread runner(engine, stop_flag);
			Thread::sleep_ms(100);

			WsTestClient ws_client;
			REQUIRE(ws_client.connect("127.0.0.1", telemetry_port, "/api/telemetry/ws"));
			HeapVector<char> hello_text;
			REQUIRE(wait_for_ws_text_message(ws_client, hello_text));
			HeapVector<char> layout_text;
			REQUIRE(wait_for_ws_text_message(ws_client, layout_text));
			json::Document layout_document;
			REQUIRE(layout_document.parse(layout_text.data()));
			const json::Value layout = layout_document.root();
			REQUIRE(layout.contains("types"));
			const json::Value types = layout["types"];
			REQUIRE(types.is_array());

			json::Value enum_it;
			types.for_each_array(
				[&](const json::Value type_json)
				{
					if (!enum_it.is_valid() && type_json.contains("name") && type_json["name"].equals("LayoutTestEnum"))
					{
						enum_it = type_json;
					}
				});
			REQUIRE(enum_it.is_valid());

			REQUIRE(enum_it.contains("enum_values"));
			const json::Value enum_values = enum_it["enum_values"];
			REQUIRE(enum_values.size() == 3);
			CHECK(enum_values[static_cast<size_t>(0)]["name"].equals("Alpha"));
			CHECK(enum_values[static_cast<size_t>(0)]["value"].get_int64() == 0);
			CHECK(enum_values[static_cast<size_t>(1)]["name"].equals("Beta"));
			CHECK(enum_values[static_cast<size_t>(1)]["value"].get_int64() == 1);
			CHECK(enum_values[static_cast<size_t>(2)]["name"].equals("Gamma"));
			CHECK(enum_values[static_cast<size_t>(2)]["value"].get_int64() == 2);

			CHECK(enum_it["enum_underlying_size"].get_int64() == sizeof(LayoutTestEnum));
			CHECK(enum_it["enum_is_flags"].get_bool() == false);

			stop_flag.set();
		}

		SECTION("Telemetry layout gives repeated dynamic struct instances distinct output types")
		{
			Model model;
			const uint16_t telemetry_port = choose_telemetry_port();
			model.set_telemetry_port(telemetry_port);
			static const FieldConfigEntry config[] = {{"max_output_bytes", "64"}};
			static const WorkloadSeed first_seed{TypeId("LargeDynamicWorkload"), StringView("first_dynamic"), 30.0f, {}, config, {}};
			static const WorkloadSeed second_seed{TypeId("LargeDynamicWorkload"), StringView("second_dynamic"), 30.0f, {}, config, {}};
			static const WorkloadSeed root_seed{TypeId("TickCounterWorkload"), StringView("dynamic_layout_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const workloads[] = {&first_seed, &second_seed, &root_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(root_seed);

			Engine engine;
			engine.load(model);
			AtomicFlag stop_flag{false};
			EngineRunThread runner(engine, stop_flag);
			Thread::sleep_ms(100);

			char url[256];
			::snprintf(url, sizeof(url), "http://127.0.0.1:%u/api/telemetry/workloads_buffer/layout", static_cast<unsigned int>(telemetry_port));
			const HttpResponse layout_response = http_request(url);
			REQUIRE(layout_response.status_code == 200);

			json::Document layout_document;
			REQUIRE(layout_document.parse(layout_response.body.data));
			const json::Value layout = layout_document.root();
			REQUIRE(layout.contains("workloads"));
			REQUIRE(layout.contains("types"));

			auto find_named = [](const json::Value& array_value, const char* name)
			{
				json::Value found;
				array_value.for_each_array(
					[&](const json::Value item)
					{
						if (!found.is_valid() && item.contains("name") && item["name"].equals(name))
						{
							found = item;
						}
					});
				return found;
			};

			auto find_field = [](const json::Value& type_value, const char* name)
			{
				json::Value found;
				if (!type_value.contains("fields"))
				{
					return found;
				}

				type_value["fields"].for_each_array(
					[&](const json::Value item)
					{
						if (!found.is_valid() && item.contains("name") && item["name"].equals(name))
						{
							found = item;
						}
					});
				return found;
			};

			const json::Value first_workload = find_named(layout["workloads"], "first_dynamic");
			const json::Value second_workload = find_named(layout["workloads"], "second_dynamic");
			REQUIRE(first_workload.is_valid());
			REQUIRE(second_workload.is_valid());
			REQUIRE(first_workload.contains("outputs"));
			REQUIRE(second_workload.contains("outputs"));
			const char* first_outputs_type_name = first_workload["outputs"]["type"].get_c_string();
			const char* second_outputs_type_name = second_workload["outputs"]["type"].get_c_string();
			REQUIRE(first_outputs_type_name != nullptr);
			REQUIRE(second_outputs_type_name != nullptr);
			CHECK(!StringView(first_outputs_type_name).equals(second_outputs_type_name));

			const json::Value first_outputs_type = find_named(layout["types"], first_outputs_type_name);
			const json::Value second_outputs_type = find_named(layout["types"], second_outputs_type_name);
			REQUIRE(first_outputs_type.is_valid());
			REQUIRE(second_outputs_type.is_valid());
			const json::Value first_bytes_field = find_field(first_outputs_type, "bytes");
			const json::Value second_bytes_field = find_field(second_outputs_type, "bytes");
			REQUIRE(first_bytes_field.is_valid());
			REQUIRE(second_bytes_field.is_valid());
			const char* first_dynamic_type_name = first_bytes_field["type"].get_c_string();
			const char* second_dynamic_type_name = second_bytes_field["type"].get_c_string();
			REQUIRE(first_dynamic_type_name != nullptr);
			REQUIRE(second_dynamic_type_name != nullptr);
			CHECK(!StringView(first_dynamic_type_name).equals(second_dynamic_type_name));

			const json::Value first_dynamic_type = find_named(layout["types"], first_dynamic_type_name);
			const json::Value second_dynamic_type = find_named(layout["types"], second_dynamic_type_name);
			REQUIRE(first_dynamic_type.is_valid());
			REQUIRE(second_dynamic_type.is_valid());
			const json::Value first_data_buffer = find_field(first_dynamic_type, "data_buffer");
			const json::Value second_data_buffer = find_field(second_dynamic_type, "data_buffer");
			REQUIRE(first_data_buffer.is_valid());
			REQUIRE(second_data_buffer.is_valid());
			CHECK(first_data_buffer["offset_within_container"].get_int64() != second_data_buffer["offset_within_container"].get_int64());

			stop_flag.set();
		}

		SECTION("WorkloadsBuffer is exact-sized after dynamic-struct preflight")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const FieldConfigEntry config[] = {{"max_output_bytes", "5242880"}};
			static const WorkloadSeed workload_seed{TypeId("LargeDynamicWorkload"), StringView("large_dynamic"), 30.0f, {}, config, {}};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer();
			REQUIRE(workloads_buffer.get_size() == workloads_buffer.get_size_used());
			REQUIRE(workloads_buffer.get_size_used() > (size_t)(4 * 1024 * 1024));

			const auto* instance = engine.find_instance<LargeDynamicWorkload>(workload_seed.unique_name);
			REQUIRE(instance != nullptr);
			REQUIRE(instance->outputs.bytes.capacity() == 5 * 1024 * 1024);
		}

		SECTION("start_fn executes on same thread as tick_fn")
		{
			Model model;
			model.set_telemetry_port(choose_telemetry_port());
			static const WorkloadSeed workload_seed{
				TypeId("ThreadAffinityWorkload"),
				StringView("affinity"),
				120.0f,
				{}, // children
				{}, // config
				{}	// inputs
			};
			static const WorkloadSeed* const workloads[] = {&workload_seed};
			model.use_workload_seeds(workloads);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_flag{false};
			EngineRunThread runner(engine, stop_flag);

			Thread::sleep_ms(30);
			stop_flag.set();

			const auto* info = engine.find_instance<ThreadAffinityWorkload>(workload_seed.unique_name);
			REQUIRE(info != nullptr);
			REQUIRE(info->tick_count.load() > 0);
			CHECK(info->start_thread == info->first_tick_thread);
			CHECK(info->start_thread != Thread::ThreadId{});
		}

		SECTION("Telemetry websocket sends hello, layout, frame, and write response")
		{
			const uint16_t telemetry_port = choose_telemetry_port();
			REQUIRE(telemetry_port != 0);

			Model model;
			model.set_model_name("ws-local-model");
			model.set_telemetry_port(telemetry_port);
			static const FieldConfigEntry local_inputs[] = {{"input_float", "1.5"}, {"input_string_64", "seed"}};
			static const WorkloadSeed local_root{TypeId("TickCounterWorkload"), StringView("ws_local_root"), 60.0f, {}, {}, {}};
			static const WorkloadSeed local_dummy{TypeId("DummyWorkload"), StringView("ws_dummy"), 30.0f, {}, {}, local_inputs};
			static const WorkloadSeed* const local_workloads[] = {&local_dummy, &local_root};
			model.use_workload_seeds(local_workloads);
			model.set_root_workload(local_root);

			Engine engine;
			engine.load(model);
			const DummyWorkload* dummy_instance = engine.find_instance<DummyWorkload>("ws_dummy");
			REQUIRE(dummy_instance != nullptr);

			AtomicFlag stop_flag{false};
			EngineRunThread runner(engine, stop_flag);
			Thread::sleep_ms(100);

			WsTestClient ws_client;
			REQUIRE(ws_client.connect("127.0.0.1", telemetry_port, "/api/telemetry/ws"));

			HeapVector<char> hello_text;
			REQUIRE(wait_for_ws_text_message(ws_client, hello_text));
			json::Document hello_document;
			REQUIRE(hello_document.parse(hello_text.data()));
			const json::Value hello = hello_document.root();
			REQUIRE(hello.is_object());
			REQUIRE(hello.contains("type"));
			REQUIRE(hello["type"].equals("hello"));
			REQUIRE(hello.contains("model_id"));
			REQUIRE(hello["model_id"].equals("ws-local-model"));
			REQUIRE(hello.contains("engine_session_id"));
			const char* hello_session_id = hello["engine_session_id"].get_c_string();
			REQUIRE(hello_session_id != nullptr);
			REQUIRE(hello_session_id[0] != '\0');

			HeapVector<char> layout_text;
			REQUIRE(wait_for_ws_text_message(ws_client, layout_text));
			json::Document layout_document;
			REQUIRE(layout_document.parse(layout_text.data()));
			const json::Value layout = layout_document.root();
			REQUIRE(layout.is_object());
			REQUIRE(layout.contains("writable_inputs"));
			REQUIRE(layout.contains("engine_session_id"));
			const char* layout_session_id = layout["engine_session_id"].get_c_string();
			REQUIRE(layout_session_id != nullptr);
			REQUIRE(layout_session_id[0] != '\0');
			CHECK(StringView(layout_session_id).equals(hello_session_id));

			const json::Value writable_inputs = layout["writable_inputs"];
			REQUIRE(writable_inputs.is_array());
			json::Value float_writable;
			writable_inputs.for_each_array(
				[&](const json::Value writable)
				{
					if (!float_writable.is_valid() && writable.contains("field_path") &&
						contains_text(writable["field_path"].get_c_string(), "input_float"))
					{
						float_writable = writable;
					}
				});
			REQUIRE(float_writable.is_valid());
			const uint32_t float_handle = static_cast<uint32_t>(float_writable["field_handle"].get_uint64());

			json::Document frame_metadata_document;
			size_t frame_payload_size = 0;
			REQUIRE(wait_for_ws_frame_metadata_and_payload(ws_client, frame_metadata_document, frame_payload_size));
			const json::Value frame_metadata = frame_metadata_document.root();
			REQUIRE(frame_metadata.is_object());
			REQUIRE(frame_metadata.contains("type"));
			REQUIRE(frame_metadata["type"].equals("frame"));
			REQUIRE(frame_metadata.contains("frame_seq"));
			CHECK((frame_metadata["frame_seq"].get_uint64() % 2) == 0);
			REQUIRE(frame_metadata.contains("engine_session_id"));
			CHECK(StringView(frame_metadata["engine_session_id"].get_c_string()).equals(layout_session_id));
			REQUIRE(frame_payload_size > 0);

			char write_body[512];
			::snprintf(write_body,
				sizeof(write_body),
				"{\"engine_session_id\":\"%s\",\"writes\":[{\"field_handle\":%u,\"value\":6.5}]}",
				layout_session_id,
				static_cast<unsigned>(float_handle));
			REQUIRE(ws_client.send_text(write_body));

			bool saw_write_response = false;
			WsFrame ws_frame;
			for (int i = 0; i < 24; ++i)
			{
				if (!read_ws_application_frame(ws_client, ws_frame))
				{
					break;
				}
				if (ws_frame.opcode != 0x1)
				{
					continue;
				}
				HeapVector<char> text;
				if (!frame_to_text(ws_frame, text))
				{
					continue;
				}
				if (contains_text(text.data(), "\"accepted_count\""))
				{
					saw_write_response = true;
					break;
				}
			}
			REQUIRE(saw_write_response);

			Thread::sleep_ms(80);
			CHECK(dummy_instance->inputs.input_float == Catch::Approx(6.5f));

			stop_flag.set();
		}

		SECTION("Telemetry gateway websocket forwards peer layout, frame, and writes")
		{
			const uint16_t peer_port = choose_telemetry_port();
			const uint16_t gateway_port = choose_telemetry_port();
			REQUIRE(peer_port != 0);
			REQUIRE(gateway_port != 0);
			REQUIRE(peer_port != gateway_port);

			Model peer_model;
			peer_model.set_model_name("peer-model");
			peer_model.set_telemetry_port(peer_port);
			static const FieldConfigEntry peer_inputs[] = {{"input_float", "1.0"}, {"input_string_64", "hello"}};
			static const WorkloadSeed peer_root{TypeId("TickCounterWorkload"), StringView("peer_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed peer_workload{TypeId("DummyWorkload"), StringView("peer_dummy"), 30.0f, {}, {}, peer_inputs};
			static const WorkloadSeed* const peer_workloads[] = {&peer_workload, &peer_root};
			peer_model.use_workload_seeds(peer_workloads);
			peer_model.set_root_workload(peer_root);

			Model gateway_model;
			gateway_model.set_model_name("gateway-model");
			gateway_model.set_telemetry_port(gateway_port);
			gateway_model.set_telemetry_is_gateway(true);
			static const WorkloadSeed gateway_root{TypeId("TickCounterWorkload"), StringView("gateway_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const gateway_workloads[] = {&gateway_root};
			static const TelemetryPeerSeed telemetry_peer{StringView("peer-model"), StringView("127.0.0.1"), peer_port, false};
			static const TelemetryPeerSeed* const gateway_peers[] = {&telemetry_peer};
			gateway_model.use_workload_seeds(gateway_workloads);
			gateway_model.use_telemetry_peer_seeds(gateway_peers);
			gateway_model.set_root_workload(gateway_root);

			Engine peer_engine;
			peer_engine.load(peer_model);
			const DummyWorkload* peer_instance = peer_engine.find_instance<DummyWorkload>("peer_dummy");
			REQUIRE(peer_instance != nullptr);
			AtomicFlag peer_stop{false};
			EngineRunThread peer_runner(peer_engine, peer_stop);

			Engine gateway_engine;
			gateway_engine.load(gateway_model);
			AtomicFlag gateway_stop{false};
			EngineRunThread gateway_runner(gateway_engine, gateway_stop);

			Thread::sleep_ms(100);

			WsTestClient ws_client;
			REQUIRE(ws_client.connect("127.0.0.1", gateway_port, "/api/telemetry-gateway/peer-model/ws"));

			HeapVector<char> hello_text;
			REQUIRE(wait_for_ws_text_message(ws_client, hello_text));
			json::Document hello_document;
			REQUIRE(hello_document.parse(hello_text.data()));
			const json::Value hello = hello_document.root();
			REQUIRE(hello.is_object());
			REQUIRE(hello.contains("type"));
			REQUIRE(hello["type"].equals("hello"));
			REQUIRE(hello.contains("model_id"));
			REQUIRE(hello["model_id"].equals("peer-model"));

			HeapVector<char> layout_text;
			REQUIRE(wait_for_ws_text_message(ws_client, layout_text));
			json::Document layout_document;
			REQUIRE(layout_document.parse(layout_text.data()));
			const json::Value layout = layout_document.root();
			REQUIRE(layout.is_object());
			REQUIRE(layout.contains("engine_session_id"));
			REQUIRE(layout.contains("writable_inputs"));
			const char* layout_session_id = layout["engine_session_id"].get_c_string();
			REQUIRE(layout_session_id != nullptr);
			REQUIRE(layout_session_id[0] != '\0');

			const json::Value writable_inputs = layout["writable_inputs"];
			REQUIRE(writable_inputs.is_array());
			json::Value float_writable;
			writable_inputs.for_each_array(
				[&](const json::Value writable)
				{
					if (!float_writable.is_valid() && writable.contains("field_path") &&
						contains_text(writable["field_path"].get_c_string(), "input_float"))
					{
						float_writable = writable;
					}
				});
			REQUIRE(float_writable.is_valid());
			const uint32_t float_handle = static_cast<uint32_t>(float_writable["field_handle"].get_uint64());

			json::Document frame_metadata_document;
			size_t frame_payload_size = 0;
			REQUIRE(wait_for_ws_frame_metadata_and_payload(ws_client, frame_metadata_document, frame_payload_size));
			const json::Value frame_metadata = frame_metadata_document.root();
			REQUIRE(frame_metadata.is_object());
			REQUIRE(frame_metadata.contains("type"));
			REQUIRE(frame_metadata["type"].equals("frame"));
			REQUIRE(frame_metadata.contains("frame_seq"));
			CHECK((frame_metadata["frame_seq"].get_uint64() % 2) == 0);
			REQUIRE(frame_metadata.contains("engine_session_id"));
			CHECK(StringView(frame_metadata["engine_session_id"].get_c_string()).equals(layout_session_id));
			REQUIRE(frame_payload_size > 0);

			char write_body[512];
			::snprintf(write_body,
				sizeof(write_body),
				"{\"engine_session_id\":\"%s\",\"writes\":[{\"field_handle\":%u,\"value\":19.75}]}",
				layout_session_id,
				static_cast<unsigned>(float_handle));
			REQUIRE(ws_client.send_text(write_body));

			bool saw_write_response = false;
			WsFrame ws_frame;
			for (int i = 0; i < 24; ++i)
			{
				if (!read_ws_application_frame(ws_client, ws_frame))
				{
					break;
				}
				if (ws_frame.opcode != 0x1)
				{
					continue;
				}
				HeapVector<char> text;
				if (!frame_to_text(ws_frame, text))
				{
					continue;
				}
				if (contains_text(text.data(), "\"accepted_count\""))
				{
					saw_write_response = true;
					break;
				}
			}
			REQUIRE(saw_write_response);

			Thread::sleep_ms(80);
			CHECK(peer_instance->inputs.input_float == Catch::Approx(19.75f));

			gateway_stop.set();
			peer_stop.set();
		}

		SECTION("Telemetry gateway models endpoint remains available")
		{
			const uint16_t peer_port = choose_telemetry_port();
			const uint16_t gateway_port = choose_telemetry_port();
			REQUIRE(peer_port != 0);
			REQUIRE(gateway_port != 0);
			REQUIRE(peer_port != gateway_port);

			Model peer_model;
			peer_model.set_model_name("peer-model");
			peer_model.set_telemetry_port(peer_port);
			static const WorkloadSeed peer_root{TypeId("TickCounterWorkload"), StringView("peer_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const peer_workloads[] = {&peer_root};
			peer_model.use_workload_seeds(peer_workloads);
			peer_model.set_root_workload(peer_root);

			Model gateway_model;
			gateway_model.set_model_name("gateway-model");
			gateway_model.set_telemetry_port(gateway_port);
			gateway_model.set_telemetry_is_gateway(true);
			static const WorkloadSeed gateway_root{TypeId("TickCounterWorkload"), StringView("gateway_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const gateway_workloads[] = {&gateway_root};
			static const TelemetryPeerSeed telemetry_peer{StringView("peer-model"), StringView("127.0.0.1"), peer_port, false};
			static const TelemetryPeerSeed* const gateway_peers[] = {&telemetry_peer};
			gateway_model.use_workload_seeds(gateway_workloads);
			gateway_model.use_telemetry_peer_seeds(gateway_peers);
			gateway_model.set_root_workload(gateway_root);

			Engine peer_engine;
			peer_engine.load(peer_model);
			AtomicFlag peer_stop{false};
			EngineRunThread peer_runner(peer_engine, peer_stop);

			Engine gateway_engine;
			gateway_engine.load(gateway_model);
			AtomicFlag gateway_stop{false};
			EngineRunThread gateway_runner(gateway_engine, gateway_stop);

			Thread::sleep_ms(100);

			char url[256];
			::snprintf(url, sizeof(url), "http://127.0.0.1:%u/api/telemetry-gateway/models", static_cast<unsigned int>(gateway_port));
			const HttpResponse models_response = http_request(url);
			REQUIRE(models_response.status_code == 200);
			json::Document models_document;
			REQUIRE(models_document.parse(models_response.body.data));
			const json::Value models_json = models_document.root();
			REQUIRE(models_json.contains("models"));
			const json::Value models = models_json["models"];
			bool saw_gateway_model = false;
			bool saw_peer_model = false;
			models.for_each_array(
				[&](const json::Value model_json)
				{
					if (model_json["model_id"].equals("gateway-model"))
					{
						saw_gateway_model = true;
					}
					if (model_json["model_id"].equals("peer-model"))
					{
						saw_peer_model = true;
					}
				});
			REQUIRE(saw_gateway_model);
			REQUIRE(saw_peer_model);

			gateway_stop.set();
			peer_stop.set();
		}

		SECTION("Telemetry REST layout endpoints remain available and stale gateway writes are rejected")
		{
			const uint16_t peer_port = choose_telemetry_port();
			const uint16_t gateway_port = choose_telemetry_port();
			REQUIRE(peer_port != 0);
			REQUIRE(gateway_port != 0);
			REQUIRE(peer_port != gateway_port);

			Model peer_model;
			peer_model.set_model_name("peer-model");
			peer_model.set_telemetry_port(peer_port);
			static const WorkloadSeed peer_root{TypeId("TickCounterWorkload"), StringView("peer_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const peer_workloads[] = {&peer_root};
			peer_model.use_workload_seeds(peer_workloads);
			peer_model.set_root_workload(peer_root);

			Model gateway_model;
			gateway_model.set_model_name("gateway-model");
			gateway_model.set_telemetry_port(gateway_port);
			gateway_model.set_telemetry_is_gateway(true);
			static const WorkloadSeed gateway_root{TypeId("TickCounterWorkload"), StringView("gateway_root"), 30.0f, {}, {}, {}};
			static const WorkloadSeed* const gateway_workloads[] = {&gateway_root};
			static const TelemetryPeerSeed telemetry_peer{StringView("peer-model"), StringView("127.0.0.1"), peer_port, false};
			static const TelemetryPeerSeed* const gateway_peers[] = {&telemetry_peer};
			gateway_model.use_workload_seeds(gateway_workloads);
			gateway_model.use_telemetry_peer_seeds(gateway_peers);
			gateway_model.set_root_workload(gateway_root);

			Engine peer_engine;
			peer_engine.load(peer_model);
			AtomicFlag peer_stop{false};
			EngineRunThread peer_runner(peer_engine, peer_stop);

			Engine gateway_engine;
			gateway_engine.load(gateway_model);
			AtomicFlag gateway_stop{false};
			EngineRunThread gateway_runner(gateway_engine, gateway_stop);

			Thread::sleep_ms(100);

			char url[256];
			::snprintf(url, sizeof(url), "http://127.0.0.1:%u/api/telemetry/workloads_buffer/layout", static_cast<unsigned int>(gateway_port));
			const HttpResponse local_layout = http_request(url);
			REQUIRE(local_layout.status_code == 200);
			REQUIRE(contains_text(local_layout.body.data, "\"writable_inputs\""));

			::snprintf(url,
				sizeof(url),
				"http://127.0.0.1:%u/api/telemetry-gateway/peer-model/workloads_buffer/layout",
				static_cast<unsigned int>(gateway_port));
			const HttpResponse peer_layout = http_request(url);
			REQUIRE(peer_layout.status_code == 200);
			REQUIRE(contains_text(peer_layout.body.data, "\"writable_inputs\""));

			const char* schema_path = resolve_layout_schema_path();
			REQUIRE(schema_path != nullptr);
			char validation_error[256] = {};
			INFO(validation_error);
			CHECK(validate_json_against_schema(schema_path, local_layout.body.data, validation_error, sizeof(validation_error)));
			INFO(validation_error);
			CHECK(validate_json_against_schema(schema_path, peer_layout.body.data, validation_error, sizeof(validation_error)));

			::snprintf(url,
				sizeof(url),
				"http://127.0.0.1:%u/api/telemetry-gateway/peer-model/set_workload_input_fields_data",
				static_cast<unsigned int>(gateway_port));
			const HttpResponse peer_write =
				http_request(url, "POST", "{\"engine_session_id\":\"x\",\"writes\":[{\"field_handle\":1,\"value\":1.0}]}");
			REQUIRE(peer_write.status_code == 412);
			REQUIRE(contains_text(peer_write.body.data, "\"field_path_required_for_stale_session_write\""));

			gateway_stop.set();
			peer_stop.set();
		}
	}

} // namespace robotick::test
