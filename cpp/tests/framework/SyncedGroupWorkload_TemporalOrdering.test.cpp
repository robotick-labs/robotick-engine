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

	TEST_CASE("Unit|Framework|SyncedGroupWorkload|Data connections are propagated before children tick")
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

		const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
		group_info.type->start_fn(group_info.ptr, tick_rate);

		const int num_ticks = 5;
		const double dt = 1.0 / tick_rate;
		for (int i = 0; i < num_ticks; ++i)
		{
			group_info.type->tick_fn(group_info.ptr, dt);
			std::this_thread::sleep_for(std::chrono::duration<double>(dt));
		}

		group_info.type->stop_fn(group_info.ptr);

		const auto& receiver_info = EngineInspector::get_instance_info(engine, receiver.index);
		const auto* workload = static_cast<const ReceiverWorkload*>((void*)receiver_info.ptr);
		REQUIRE(workload->received.size() == num_ticks);

		for (size_t i = 0; i < workload->received.size(); ++i)
		{
			INFO("Received[" << i << "] = " << workload->received[i]);
			CHECK(workload->received[i] == static_cast<int>(i + 1)); // sender increments from 0
		}
	}
} // namespace robotick::test
