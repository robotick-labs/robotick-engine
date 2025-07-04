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
using namespace robotick::test;

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
		const const WorkloadSeed& child1 = model.add("DummyTickingWorkload", "child1", 50.0);
		const const WorkloadSeed& child2 = model.add("DummyTickingWorkload", "child2", 50.0);
		const auto group = model.add("SequencedGroupWorkload", "group", {child1, child2}, 50.0);
		model.set_root_workload(group);

		Engine engine;
		engine.load(model);

		const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
		auto* group_ptr = group_info.get_ptr(engine);
		REQUIRE(group_ptr != nullptr);

		REQUIRE_NOTHROW(group_info.type->start_fn(group_ptr, 50.0));
		REQUIRE_NOTHROW(group_info.type->tick_fn(group_ptr, TICK_INFO_FIRST_10MS_100HZ));
		REQUIRE_NOTHROW(group_info.type->stop_fn(group_ptr));

		CHECK(DummyTickingWorkload::tick_count == 2);
	}

	SECTION("Overrun logs if exceeded")
	{
		Model model;
		const auto handle = model.add("SlowTickWorkload", "slow", 50.0);
		const auto group = model.add("SequencedGroupWorkload", "group", {handle}, 1000.0);
		model.set_root_workload(group);

		Engine engine;
		engine.load(model);

		const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
		REQUIRE_NOTHROW(group_info.type->tick_fn(group_info.get_ptr(engine), TICK_INFO_FIRST_1MS_1KHZ)); // 1ms budget, expect warning log
	}
}
