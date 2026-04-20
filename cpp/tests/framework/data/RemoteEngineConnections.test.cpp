// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/RemoteModelSeed.h"
#include "robotick/framework/time/Clock.h"

#include <arpa/inet.h>
#include <catch2/catch_all.hpp>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace robotick::test
{
	namespace
	{
		struct EngineRunContext
		{
			Engine* engine = nullptr;
			AtomicFlag* stop_flag = nullptr;
		};

		struct EngineRunThreadsGuard
		{
			AtomicFlag* stop_flag = nullptr;
			Thread* sender_thread = nullptr;
			Thread* receiver_thread = nullptr;

			~EngineRunThreadsGuard()
			{
				if (stop_flag)
				{
					stop_flag->set();
				}
				if (sender_thread && sender_thread->is_joining_supported())
				{
					sender_thread->join();
				}
				if (receiver_thread && receiver_thread->is_joining_supported())
				{
					receiver_thread->join();
				}
			}
		};

		struct HttpTextBuffer
		{
			char data[65536] = {};
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
		};

		size_t curl_write_to_text(char* ptr, size_t size, size_t nmemb, void* userdata)
		{
			auto& out = *static_cast<HttpTextBuffer*>(userdata);
			const size_t bytes = size * nmemb;
			out.append(ptr, bytes);
			return bytes;
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

		bool contains_text(const char* haystack, const char* needle)
		{
			return haystack && needle && ::strstr(haystack, needle) != nullptr;
		}

		bool telemetry_raw_int_equals(uint16_t telemetry_port, size_t offset, int expected)
		{
			char raw_url[256];
			::snprintf(raw_url, sizeof(raw_url), "http://127.0.0.1:%u/api/telemetry/workloads_buffer/raw", static_cast<unsigned int>(telemetry_port));
			const HttpResponse raw = http_request(raw_url);
			if (raw.status_code != 200 || raw.body.len < offset + sizeof(int))
			{
				return false;
			}

			int actual = 0;
			::memcpy(&actual, raw.body.data + offset, sizeof(actual));
			return actual == expected;
		}

		bool wait_for_telemetry_raw_int(uint16_t telemetry_port, size_t offset, int expected, int attempts)
		{
			for (int i = 0; i < attempts; ++i)
			{
				if (telemetry_raw_int_equals(telemetry_port, offset, expected))
				{
					return true;
				}
				Thread::sleep_ms(10);
			}
			return false;
		}

		constexpr long long kRemoteEngineConnectionTimeoutNs = 3'000'000'000LL;

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
			ROBOTICK_FATAL_EXIT("RemoteEngineConnections test: no free telemetry port found after 3 attempts");
			return 0;
		}

		uint16_t choose_telemetry_port_distinct_from(uint16_t existing_port)
		{
			for (int attempt = 0; attempt < 8; ++attempt)
			{
				const uint16_t candidate = choose_telemetry_port();
				if (candidate != 0 && candidate != existing_port)
				{
					return candidate;
				}
			}
			ROBOTICK_FATAL_EXIT(
				"RemoteEngineConnections test: failed to choose distinct telemetry port from %u",
				static_cast<unsigned>(existing_port));
			return 0;
		}

		void engine_run_entry(void* arg)
		{
			auto* ctx = static_cast<EngineRunContext*>(arg);
			ctx->engine->run(*ctx->stop_flag);
			delete ctx;
		}
	} // namespace

	// === Test workload + structs ===

	static constexpr int VALUE_TO_TRANSMIT = 123;

	struct RemoteInputs
	{
		int remote_x = 0;
		int local_x = VALUE_TO_TRANSMIT;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteInputs)
	ROBOTICK_STRUCT_FIELD(RemoteInputs, int, remote_x)
	ROBOTICK_STRUCT_FIELD(RemoteInputs, int, local_x)
	ROBOTICK_REGISTER_STRUCT_END(RemoteInputs)

	struct RemoteOutputs
	{
		int local_x = VALUE_TO_TRANSMIT;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteOutputs)
	ROBOTICK_STRUCT_FIELD(RemoteOutputs, int, local_x)
	ROBOTICK_REGISTER_STRUCT_END(RemoteOutputs)

	struct TestRemoteWorkload
	{
		RemoteInputs inputs;
		RemoteOutputs outputs;

		void tick(const TickInfo&) { outputs.local_x = inputs.local_x; }
	};
	ROBOTICK_REGISTER_WORKLOAD(TestRemoteWorkload, void, RemoteInputs, RemoteOutputs)

	// === Test case ===

	TEST_CASE("Integration/Framework/RemoteEngineConnections", "[RemoteEngineConnections]")
	{
		// --- Setup sender
		Model sender_model;
		sender_model.set_model_name("sender_model");

		static const WorkloadSeed sender_seed{
			TypeId("TestRemoteWorkload"),
			StringView("sender_workload"),
			100.0f,
			{}, // children
			{}, // config
			{}	// inputs
		};
		static const WorkloadSeed* const sender_workloads[] = {&sender_seed};
		static const DataConnectionSeed remote_connection{"sender_workload.outputs.local_x", "receiver_workload.inputs.remote_x"};
		static const DataConnectionSeed* const remote_connections[] = {&remote_connection};
		static const RemoteModelSeed remote_receiver = []() {
			RemoteModelSeed seed{
				StringView("receiver_model"),
				ArrayView<const DataConnectionSeed*>(remote_connections)
			};
			seed.comms_mode = RemoteModelSeed::Mode::IP;
			seed.comms_channel = StringView("ip:127.0.0.1");
			return seed;
		}();
		static const RemoteModelSeed* const remote_models[] = {&remote_receiver};

		const uint16_t sender_telemetry_port = choose_telemetry_port();
		const uint16_t receiver_telemetry_port = choose_telemetry_port_distinct_from(sender_telemetry_port);
		sender_model.use_workload_seeds(sender_workloads);
		sender_model.use_remote_models(remote_models);
		sender_model.set_root_workload(sender_seed);
		sender_model.set_telemetry_port(sender_telemetry_port);

		// --- Setup receiver
		Model receiver_model;
		receiver_model.set_model_name("receiver_model");
		static const WorkloadSeed receiver_seed{
			TypeId("TestRemoteWorkload"),
			StringView("receiver_workload"),
			100.0f,
			{}, // children
			{}, // config
			{}	// inputs
		};
		static const WorkloadSeed* const receiver_workloads[] = {&receiver_seed};
		receiver_model.set_telemetry_port(receiver_telemetry_port);
		receiver_model.use_workload_seeds(receiver_workloads);
		receiver_model.set_root_workload(receiver_seed);

		// --- Load both engines
		Engine sender;
		Engine receiver;

		sender.load(sender_model);
		receiver.load(receiver_model);

		auto* receiver_workload = receiver.find_instance<TestRemoteWorkload>("receiver_workload");
		REQUIRE(receiver_workload != nullptr);
		const uint8_t* receiver_buffer_base = receiver.get_workloads_buffer().raw_ptr();
		const size_t receiver_remote_x_offset =
			static_cast<size_t>(reinterpret_cast<const uint8_t*>(&receiver_workload->inputs.remote_x) - receiver_buffer_base);

		AtomicFlag stop_flag(false);
		AtomicFlag success_flag(false);

		// --- Threaded run
		Thread sender_thread(engine_run_entry, new EngineRunContext{&sender, &stop_flag});
		Thread receiver_thread(engine_run_entry, new EngineRunContext{&receiver, &stop_flag});
		EngineRunThreadsGuard thread_guard{&stop_flag, &sender_thread, &receiver_thread};

		// --- Watch for success or timeout
		const auto start = Clock::now();

		while (!stop_flag.is_set())
		{
			if (telemetry_raw_int_equals(receiver_telemetry_port, receiver_remote_x_offset, VALUE_TO_TRANSMIT))
			{
				success_flag.set();
				break;
			}

			const auto elapsed_ns = Clock::to_nanoseconds(Clock::now() - start).count();
			if (elapsed_ns > kRemoteEngineConnectionTimeoutNs)
				break;

			Thread::sleep_ms(10);
		}

		REQUIRE(success_flag.is_set());

		{
			char layout_url[256];
			::snprintf(
				layout_url,
				sizeof(layout_url),
				"http://127.0.0.1:%u/api/telemetry/workloads_buffer/layout",
				static_cast<unsigned int>(receiver_telemetry_port));
			const HttpResponse layout = http_request(layout_url);
			REQUIRE(layout.status_code == 200);
			REQUIRE(contains_text(layout.body.data, "\"field_path\":\"receiver_workload.inputs.remote_x\""));
			REQUIRE(contains_text(layout.body.data, "\"incoming_connection_handle\":"));
			REQUIRE(contains_text(layout.body.data, "\"incoming_connection_enabled\":true"));
		}

		{
			char state_url[256];
			::snprintf(
				state_url,
				sizeof(state_url),
				"http://127.0.0.1:%u/api/telemetry/set_workload_input_connection_state",
				static_cast<unsigned int>(receiver_telemetry_port));
			const HttpResponse suppress = http_request(
				state_url,
				"POST",
				"{\"updates\":[{\"field_path\":\"receiver_workload.inputs.remote_x\",\"enabled\":false}]}");
			REQUIRE(suppress.status_code == 200);
			REQUIRE(contains_text(suppress.body.data, "\"incoming_connection_enabled\":false"));

			char sender_write_url[256];
			::snprintf(sender_write_url,
				sizeof(sender_write_url),
				"http://127.0.0.1:%u/api/telemetry/set_workload_input_fields_data",
				static_cast<unsigned int>(sender_telemetry_port));
			const HttpResponse sender_write =
				http_request(sender_write_url, "POST", "{\"writes\":[{\"field_path\":\"sender_workload.inputs.local_x\",\"value\":456}]}");
			REQUIRE(sender_write.status_code == 200);

			for (int i = 0; i < 40 && !telemetry_raw_int_equals(receiver_telemetry_port, receiver_remote_x_offset, 456); ++i)
			{
				Thread::sleep_ms(10);
			}
			REQUIRE_FALSE(telemetry_raw_int_equals(receiver_telemetry_port, receiver_remote_x_offset, 456));
			REQUIRE(telemetry_raw_int_equals(receiver_telemetry_port, receiver_remote_x_offset, VALUE_TO_TRANSMIT));

			const HttpResponse reenable = http_request(
				state_url,
				"POST",
				"{\"updates\":[{\"field_path\":\"receiver_workload.inputs.remote_x\",\"enabled\":true}]}");
			REQUIRE(reenable.status_code == 200);
			REQUIRE(contains_text(reenable.body.data, "\"incoming_connection_enabled\":true"));

			REQUIRE(wait_for_telemetry_raw_int(receiver_telemetry_port, receiver_remote_x_offset, 456, 80));
		}

		REQUIRE(success_flag.is_set());
	}

} // namespace robotick::test
