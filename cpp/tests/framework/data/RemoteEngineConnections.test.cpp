// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/framework/Engine.h"
#include "robotick/platform/Threading.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	// === Test workload + structs ===

	struct RemoteInputs
	{
		int remote_x = 0;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(RemoteInputs)
	ROBOTICK_STRUCT_FIELD(RemoteInputs, int, remote_x)
	ROBOTICK_REGISTER_STRUCT_END(RemoteInputs)

	struct RemoteOutputs
	{
		int local_x = 123;
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
		// --- Setup brain (sender)
		Model brain_model;
		const auto& brain_seed = brain_model.add("TestRemoteWorkload", "remote_tx").set_tick_rate_hz(100.0f);
		brain_model.set_root_workload(brain_seed);
		brain_model.set_model_name("brain_model");

		// --- Setup spine (receiver)
		Model spine_model;
		const auto& spine_seed = spine_model.add("TestRemoteWorkload", "remote_rx").set_tick_rate_hz(100.0f);
		spine_model.set_root_workload(spine_seed);
		spine_model.set_model_name("spine_model");
		spine_model.set_telemetry_port(7091); // so as to not conflict with the above

		// Declare spine expects remote data from brain
		auto& remote = spine_model.add_remote_model("brain", "ip:127.0.0.1");
		remote.connect("remote_tx.outputs.local_x", "remote_rx.inputs.remote_x");

		// --- Load both engines
		Engine brain;
		Engine spine;

		brain.load(brain_model);
		spine.load(spine_model);

		AtomicFlag stop_flag(false);
		AtomicFlag success_flag(false);

		// --- Threaded run
		std::thread brain_thread(
			[&]()
			{
				brain.run(stop_flag);
			});

		std::thread spine_thread(
			[&]()
			{
				spine.run(stop_flag);
			});

		// --- Watch for success or timeout
		auto* brain_workload = brain.find_instance<TestRemoteWorkload>("remote_tx");
		auto* spine_workload = spine.find_instance<TestRemoteWorkload>("remote_rx");

		const auto start = std::chrono::steady_clock::now();

		while (!stop_flag.is_set())
		{
			if (spine_workload && brain_workload && spine_workload->inputs.remote_x == brain_workload->outputs.local_x)
			{
				success_flag.set();
				break;
			}

			const auto now = std::chrono::steady_clock::now();
			const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
			if (elapsed_ms > 1000)
				break;

			Thread::sleep_ms(10);
		}

		stop_flag.set();

		brain_thread.join();
		spine_thread.join();

		REQUIRE(success_flag.is_set());
	}

} // namespace robotick::test
