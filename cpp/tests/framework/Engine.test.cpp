// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/platform/Threading.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

namespace robotick::test
{

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

	} // namespace

	// === Tests ===

	TEST_CASE("Unit/Framework/Engine")
	{
		SECTION("DummyWorkload stores tick rate correctly")
		{
			Model model;
			const WorkloadSeed& workload_seed = model.add("DummyWorkload", "A").set_tick_rate_hz(123.0f);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			float tick_rate = engine.get_root_instance_info()->seed->tick_rate_hz;
			REQUIRE(tick_rate == Catch::Approx(123.0f));
		}

		SECTION("DummyWorkload config is loaded via load()")
		{
			Model model;
			const WorkloadSeed& workload_seed = model.add("DummyWorkload", "A").set_tick_rate_hz(1.0f).set_config({{"value", "42"}});
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->config.value == 42);
		}

		SECTION("DummyWorkload config is loaded via load()")
		{
			Model model;
			const WorkloadSeed& workload_seed =
				model.add("DummyWorkload", "A").set_tick_rate_hz(1.0f).set_inputs({{"input_string_64", "hello there"}, {"input_float", "1.234"}});
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			const DummyWorkload* ptr = engine.find_instance<DummyWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->inputs.input_float == 1.234f);
			REQUIRE(ptr->inputs.input_string_64 == "hello there");
		}

		SECTION("Multiple workloads supported")
		{
			Model model;
			const WorkloadSeed& a = model.add("DummyWorkload", "one").set_tick_rate_hz(1.0f).set_config({{"value", "1"}});
			const WorkloadSeed& b = model.add("DummyWorkload", "two").set_tick_rate_hz(1.0f).set_config({{"value", "2"}});
			const WorkloadSeed& root = model.add("SequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&a, &b});
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
			const WorkloadSeed& workload_seed = model.add("TickCounterWorkload", "ticky").set_tick_rate_hz(200.0f);
			model.set_root_workload(workload_seed);

			Engine engine;
			engine.load(model);

			AtomicFlag stop_after_next_tick_flag{true};
			engine.run(stop_after_next_tick_flag); // will tick at least once even if stop_after_next_tick_flag is true

			const TickCounterWorkload* ptr = engine.find_instance<TickCounterWorkload>(workload_seed.unique_name);
			REQUIRE(ptr->count >= 1);
		}
	}

} // namespace robotick::test
