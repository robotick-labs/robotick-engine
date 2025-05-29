// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "utils/EngineInspector.h"
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

namespace robotick::test
{
	namespace
	{
		struct SenderOut
		{
			int output = 0;
		};
		ROBOTICK_BEGIN_FIELDS(SenderOut)
		ROBOTICK_FIELD(SenderOut, output)
		ROBOTICK_END_FIELDS()

		struct ReceiverIn
		{
			int input = 0;
		};
		ROBOTICK_BEGIN_FIELDS(ReceiverIn)
		ROBOTICK_FIELD(ReceiverIn, input)
		ROBOTICK_END_FIELDS()

		struct SenderWorkload
		{
			SenderOut outputs;
			void tick(double) { outputs.output++; }
		};
		ROBOTICK_DEFINE_WORKLOAD(SenderWorkload)

		struct ReceiverWorkload
		{
			ReceiverIn inputs;
			std::vector<int> received;
			void tick(double) { received.push_back(inputs.input); }
		};
		ROBOTICK_DEFINE_WORKLOAD(ReceiverWorkload)
	} // namespace

	TEST_CASE("Unit|Framework|Data|Connection|Data connections are propagated correctly")
	{
		Model model;
		const double tick_rate = 100.0;

		auto sender = model.add("SenderWorkload", "sender", tick_rate);
		auto receiver = model.add("ReceiverWorkload", "receiver", tick_rate);
		auto group = model.add("SyncedGroupWorkload", "group", {sender, receiver}, tick_rate);

		model.connect("sender.outputs.output", "receiver.inputs.input");
		model.set_root(group);

		Engine engine;
		engine.load(model);

		std::atomic<bool> stop_flag{false};

		std::thread runner(
			[&]
			{
				engine.run(stop_flag);
			});

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		stop_flag.store(true);
		runner.join();

		const auto& receiver_info = EngineInspector::get_instance_info(engine, receiver.index);
		const auto* receiver_workload = static_cast<const ReceiverWorkload*>((void*)receiver_info.ptr);

		REQUIRE(receiver_workload->received.size() > 10);

		for (size_t i = 0; i < receiver_workload->received.size(); ++i)
		{
			INFO("Received[" << i << "] = " << receiver_workload->received[i]);
			CHECK(receiver_workload->received[i] == static_cast<int>(i));
		}
	}

} // namespace robotick::test
