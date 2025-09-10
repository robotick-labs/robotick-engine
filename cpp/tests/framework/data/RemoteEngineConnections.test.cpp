// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/framework/Engine.h"
#include "robotick/platform/Threading.h"

#include <catch2/catch_all.hpp>
#include <thread>

namespace robotick::test
{
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

		auto& remote = sender_model.add_remote_model("receiver_model", "ip:127.0.0.1");
		remote.connect("sender_workload.outputs.local_x", "receiver_workload.inputs.remote_x");

		const auto& sender_seed = sender_model.add("TestRemoteWorkload", "sender_workload").set_tick_rate_hz(100.0f);
		sender_model.set_root_workload(sender_seed);

		// --- Setup receiver
		Model receiver_model;
		receiver_model.set_model_name("receiver_model");
		const auto& receiver_seed = receiver_model.add("TestRemoteWorkload", "receiver_workload").set_tick_rate_hz(100.0f);
		receiver_model.set_telemetry_port(7091); // so as to not conflict with the above
		receiver_model.set_root_workload(receiver_seed);

		// --- Load both engines
		Engine sender;
		Engine receiver;

		sender.load(sender_model);
		receiver.load(receiver_model);

		AtomicFlag stop_flag(false);
		AtomicFlag success_flag(false);

		// --- Threaded run
		std::thread sender_thread(
			[&]()
			{
				sender.run(stop_flag);
			});

		std::thread receiver_thread(
			[&]()
			{
				receiver.run(stop_flag);
			});

		// --- Watch for success or timeout
		auto* sender_workload = sender.find_instance<TestRemoteWorkload>("sender_workload");
		auto* receiver_workload = receiver.find_instance<TestRemoteWorkload>("receiver_workload");

		const auto start = std::chrono::steady_clock::now();

		while (!stop_flag.is_set())
		{
			if (receiver_workload && sender_workload && receiver_workload->inputs.remote_x == sender_workload->outputs.local_x)
			{
				success_flag.set();
				break;
			}

			const auto now = std::chrono::steady_clock::now();
			const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
			if (elapsed_ms > 3000)
				break;

			Thread::sleep_ms(10);
		}

		stop_flag.set();

		sender_thread.join();
		receiver_thread.join();

		REQUIRE(sender_workload->outputs.local_x == VALUE_TO_TRANSMIT);
		REQUIRE(receiver_workload->inputs.remote_x == VALUE_TO_TRANSMIT);

		REQUIRE(success_flag.is_set());
	}

} // namespace robotick::test
