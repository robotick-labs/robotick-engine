// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/framework/model/RemoteModelSeed.h"
#include "robotick/framework/time/Clock.h"

#include <arpa/inet.h>
#include <catch2/catch_all.hpp>
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
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteInputs)
	ROBOTICK_STRUCT_FIELD(RemoteInputs, int, remote_x)
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

		void tick(const TickInfo&) {}
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

		AtomicFlag stop_flag(false);
		AtomicFlag success_flag(false);

		// --- Threaded run
		Thread sender_thread(engine_run_entry, new EngineRunContext{&sender, &stop_flag});
		Thread receiver_thread(engine_run_entry, new EngineRunContext{&receiver, &stop_flag});

		// --- Watch for success or timeout
		auto* sender_workload = sender.find_instance<TestRemoteWorkload>("sender_workload");
		auto* receiver_workload = receiver.find_instance<TestRemoteWorkload>("receiver_workload");

		const auto start = Clock::now();

		while (!stop_flag.is_set())
		{
			if (receiver_workload && sender_workload && receiver_workload->inputs.remote_x == sender_workload->outputs.local_x)
			{
				success_flag.set();
				break;
			}

			const auto elapsed_ns = Clock::to_nanoseconds(Clock::now() - start).count();
			if (elapsed_ns > kRemoteEngineConnectionTimeoutNs)
				break;

			Thread::sleep_ms(10);
		}

		stop_flag.set();

		if (sender_thread.is_joining_supported())
			sender_thread.join();
		if (receiver_thread.is_joining_supported())
			receiver_thread.join();

		REQUIRE(sender_workload->outputs.local_x == VALUE_TO_TRANSMIT);
		REQUIRE(receiver_workload->inputs.remote_x == VALUE_TO_TRANSMIT);

		REQUIRE(success_flag.is_set());
	}

} // namespace robotick::test
