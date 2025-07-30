// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/platform/Threading.h"

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
		ROBOTICK_REGISTER_STRUCT_BEGIN(SenderOut)
		ROBOTICK_STRUCT_FIELD(SenderOut, int, output)
		ROBOTICK_REGISTER_STRUCT_END(SenderOut)

		struct ReceiverIn
		{
			int input = 0;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(ReceiverIn)
		ROBOTICK_STRUCT_FIELD(ReceiverIn, int, input)
		ROBOTICK_REGISTER_STRUCT_END(ReceiverIn)

		struct SenderWorkload
		{
			SenderOut outputs;
			void tick(const TickInfo&) { outputs.output++; }
		};
		ROBOTICK_REGISTER_WORKLOAD(SenderWorkload, void, void, SenderOut)

		struct ReceiverWorkload
		{
			ReceiverIn inputs;
			std::vector<int> received;
			void tick(const TickInfo&) { received.push_back(inputs.input); }
		};
		ROBOTICK_REGISTER_WORKLOAD(ReceiverWorkload, void, ReceiverIn)
	} // namespace

	TEST_CASE("Unit/Framework/Data/Connection/SyncedGroupWorklaod")
	{
		SECTION("Data connections are propagated correctly")
		{
			Model model;
			const float tick_rate = 100.0f;

			const WorkloadSeed& sender = model.add("SenderWorkload", "sender").set_tick_rate_hz(tick_rate);
			const WorkloadSeed& receiver = model.add("ReceiverWorkload", "receiver").set_tick_rate_hz(tick_rate);
			const WorkloadSeed& group_seed = model.add("SyncedGroupWorkload", "group").set_children({&sender, &receiver}).set_tick_rate_hz(tick_rate);

			model.connect("sender.outputs.output", "receiver.inputs.input");
			model.set_root_workload(group_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_after_next_tick_flag{false};

			std::thread runner(
				[&]
				{
					engine.run(stop_after_next_tick_flag);
				});

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			stop_after_next_tick_flag.set(true);
			runner.join();

			const auto& receiver_info = *engine.find_instance_info(receiver.unique_name);
			auto* receiver_workload = static_cast<ReceiverWorkload*>((void*)receiver_info.get_ptr(engine));

			REQUIRE(receiver_workload->received.size() > 10);

			size_t num_errors = 0;

			for (size_t i = 1; i < receiver_workload->received.size(); ++i)
			{
				INFO("Received[" << i << "] = " << receiver_workload->received[i]);
				num_errors += (receiver_workload->received[i] == receiver_workload->received[i - 1] + 1) ? 0 : 1;
			}

			REQUIRE(num_errors < 3); // we need to improve this in near future
		}
	}

} // namespace robotick::test
