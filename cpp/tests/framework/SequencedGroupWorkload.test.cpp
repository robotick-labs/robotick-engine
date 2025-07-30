// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/utils/TypeId.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

using namespace robotick;

namespace
{
	// === DummyTickingWorkload ===

	struct DummyTickingWorkload
	{
		inline static int tick_count = 0;
		void tick(const TickInfo&) { ++tick_count; }
		static void reset() { tick_count = 0; }
	};

	ROBOTICK_REGISTER_WORKLOAD(DummyTickingWorkload);

	// === SlowTickWorkload ===

	struct SlowTickWorkload
	{
		void tick(const TickInfo&) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
	};

	ROBOTICK_REGISTER_WORKLOAD(SlowTickWorkload);

} // namespace

TEST_CASE("Unit/Workloads/SequencedGroupWorkload")
{
	SECTION("Child ticks are invoked in sequence")
	{
		DummyTickingWorkload::reset();

		Model model;
		const WorkloadSeed& child1 = model.add("DummyTickingWorkload", "child1").set_tick_rate_hz(50.0);
		const WorkloadSeed& child2 = model.add("DummyTickingWorkload", "child2").set_tick_rate_hz(50.0);
		const WorkloadSeed& group = model.add("SequencedGroupWorkload", "group").set_children({&child1, &child2}).set_tick_rate_hz(50.0f);
		model.set_root_workload(group);

		Engine engine;
		engine.load(model);

		const auto& group_info = *engine.find_instance_info(group.unique_name);
		auto* group_ptr = group_info.get_ptr(engine);
		REQUIRE(group_ptr != nullptr);

		const WorkloadDescriptor* workload_desc = group_info.type->get_workload_desc();

		REQUIRE_NOTHROW(workload_desc->start_fn(group_ptr, 50.0f));
		REQUIRE_NOTHROW(workload_desc->tick_fn(group_ptr, TICK_INFO_FIRST_10MS_100HZ));
		REQUIRE_NOTHROW(workload_desc->stop_fn(group_ptr));

		CHECK(DummyTickingWorkload::tick_count == 2);
	}

	SECTION("Overrun logs if exceeded")
	{
		Model model;
		const WorkloadSeed& workload_seed = model.add("SlowTickWorkload", "slow").set_tick_rate_hz(50.0f);
		const WorkloadSeed& group_seed = model.add("SequencedGroupWorkload", "group").set_children({&workload_seed}).set_tick_rate_hz(1000.0f);
		model.set_root_workload(group_seed);

		Engine engine;
		engine.load(model);

		const auto* group_info = engine.find_instance_info(group_seed.unique_name);
		REQUIRE(group_info != nullptr);
		REQUIRE_NOTHROW(
			group_info->type->get_workload_desc()->tick_fn(group_info->get_ptr(engine), TICK_INFO_FIRST_1MS_1KHZ)); // 1ms budget, expect warning log
	}
}
