// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model_v1.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/TypeId.h"
#include "utils/EngineInspector.h"

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

	struct DummyTickingWrapper
	{
		DummyTickingWorkload* impl = new DummyTickingWorkload();
		~DummyTickingWrapper() { delete impl; }
		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }
	};

	struct DummyTickingRegister
	{
		DummyTickingRegister()
		{
			const WorkloadRegistryEntry entry = {"DummyTickingWorkload", GET_TYPE_ID(DummyTickingWrapper), sizeof(DummyTickingWrapper),
				alignof(DummyTickingWrapper),
				[](void* p)
				{
					new (p) DummyTickingWrapper();
				},
				[](void* p)
				{
					static_cast<DummyTickingWrapper*>(p)->~DummyTickingWrapper();
				},
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, const TickInfo& tick_info)
				{
					static_cast<DummyTickingWrapper*>(p)->tick(tick_info);
				},
				nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static DummyTickingRegister s_register_dummy;

	// === SlowTickWorkload ===

	struct SlowTickWorkload
	{
		void tick(const TickInfo&) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
	};

	struct SlowWrapper
	{
		SlowTickWorkload impl;
		void tick(const TickInfo& tick_info) { impl.tick(tick_info); }
	};

	struct SlowTickRegister
	{
		SlowTickRegister()
		{
			const WorkloadRegistryEntry entry = {"SlowTickWorkload", GET_TYPE_ID(SlowTickWorkload), sizeof(SlowTickWorkload),
				alignof(SlowTickWorkload),
				[](void* p)
				{
					new (p) SlowWrapper();
				},
				[](void* p)
				{
					static_cast<SlowWrapper*>(p)->~SlowWrapper();
				},
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, const TickInfo& tick_info)
				{
					static_cast<SlowWrapper*>(p)->tick(tick_info);
				},
				nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static SlowTickRegister s_register_slow;
} // namespace

TEST_CASE("Unit/Workloads/SequencedGroupWorkload")
{
	SECTION("Child ticks are invoked in sequence")
	{
		DummyTickingWorkload::reset();

		Model_v1 model;
		const auto child1 = model.add("DummyTickingWorkload", "child1", 50.0);
		const auto child2 = model.add("DummyTickingWorkload", "child2", 50.0);
		const auto group = model.add("SequencedGroupWorkload", "group", {child1, child2}, 50.0);
		model.set_root(group);

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
		Model_v1 model;
		const auto handle = model.add("SlowTickWorkload", "slow", 50.0);
		const auto group = model.add("SequencedGroupWorkload", "group", {handle}, 1000.0);
		model.set_root(group);

		Engine engine;
		engine.load(model);

		const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
		REQUIRE_NOTHROW(group_info.type->tick_fn(group_info.get_ptr(engine), TICK_INFO_FIRST_1MS_1KHZ)); // 1ms budget, expect warning log
	}
}
