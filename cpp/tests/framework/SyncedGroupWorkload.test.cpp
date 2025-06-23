// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/TypeId.h"
#include "utils/EngineInspector.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>
#include <vector>

using namespace robotick;
using namespace robotick::test;

namespace
{
	// === CountingWorkload ===

	struct CountingWorkload
	{
		std::atomic<int> tick_count{0};
		double last_dt{0};

		void tick(const TickInfo& tick_info)
		{
			last_dt = tick_info.delta_time;
			tick_count.fetch_add(1);
		}
	};

	struct CountingWrapper
	{
		CountingWorkload* impl = new CountingWorkload();
		~CountingWrapper() { delete impl; }
		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }
	};

	struct CountingRegister
	{
		CountingRegister()
		{
			const WorkloadRegistryEntry entry = {"CountingWorkload", GET_TYPE_ID(CountingWorkload), sizeof(CountingWrapper), alignof(CountingWrapper),
				[](void* p)
				{
					new (p) CountingWrapper();
				},
				[](void* p)
				{
					static_cast<CountingWrapper*>(p)->~CountingWrapper();
				},
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, const TickInfo& tick_info)
				{
					static_cast<CountingWrapper*>(p)->tick(tick_info);
				},
				nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static CountingRegister s_register_counting;

	// === SlowWorkload ===

	struct SlowWorkload
	{
		std::atomic<int> tick_count{0};
		void tick(const TickInfo&)
		{
			tick_count++;
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	};

	struct SlowWrapper
	{
		SlowWorkload* impl = new SlowWorkload();
		~SlowWrapper() { delete impl; }
		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }
	};

	struct SlowRegister
	{
		SlowRegister()
		{
			const WorkloadRegistryEntry entry = {"SlowWorkload", GET_TYPE_ID(SlowWorkload), sizeof(SlowWrapper), alignof(SlowWrapper),
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
	static SlowRegister s_register_slow;
} // namespace

TEST_CASE("Unit/Workloads/SyncedGroupWorkload/All children tick in parallel")
{
	const TickInfo tick_info = TICK_INFO_FIRST_10MS_100HZ;
	const double tick_rate_hz = 1.0 / tick_info.delta_time;
	const int tick_count = 5;

	Model model;
	const auto a = model.add("CountingWorkload", "a", tick_rate_hz);
	const auto b = model.add("CountingWorkload", "b", tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {a, b}, tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& info = EngineInspector::get_instance_info(engine, group.index);
	auto* group_ptr = info.get_ptr(engine);

	REQUIRE(group_ptr != nullptr);

	info.type->start_fn(group_ptr, tick_rate_hz);

	const auto& child_a = EngineInspector::get_instance_info(engine, a.index);
	const auto& child_b = EngineInspector::get_instance_info(engine, b.index);
	auto* wa = static_cast<CountingWrapper*>((void*)child_a.get_ptr(engine));
	auto* wb = static_cast<CountingWrapper*>((void*)child_b.get_ptr(engine));

	for (int i = 0; i < tick_count; ++i)
	{
		info.type->tick_fn(group_ptr, tick_info);

		std::this_thread::sleep_for(std::chrono::duration<double>(tick_info.delta_time));

		// Confirm each child has exactly i + 1 ticks after this iteration
		CHECK(wa->impl->tick_count == i + 1);
		CHECK(wb->impl->tick_count == i + 1);
	}

	info.type->stop_fn(group_ptr);
}

TEST_CASE("Unit/Workloads/SyncedGroupWorkload/Child busy flags skip ticks")
{
	using namespace std::chrono;

	const TickInfo tick_info = TICK_INFO_FIRST_10MS_100HZ;
	const double tick_rate_hz = 1.0 / tick_info.delta_time;
	constexpr int num_ticks = 5;

	Model model;
	const auto s1 = model.add("SlowWorkload", "s1", tick_rate_hz);
	const auto s2 = model.add("SlowWorkload", "s2", tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {s1, s2}, tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	auto* group_ptr = group_info.get_ptr(engine);

	REQUIRE(group_ptr != nullptr);

	group_info.type->start_fn(group_ptr, tick_rate_hz);

	for (int i = 0; i < num_ticks; ++i)
	{
		group_info.type->tick_fn(group_ptr, tick_info);
		std::this_thread::sleep_for(10ms); // Let threads get through, but not enough for all to finish
	}

	group_info.type->stop_fn(group_ptr);

	const auto& s1_info = EngineInspector::get_instance_info(engine, s1.index);
	const auto& s2_info = EngineInspector::get_instance_info(engine, s2.index);
	auto* w1 = static_cast<SlowWrapper*>((void*)s1_info.get_ptr(engine));
	auto* w2 = static_cast<SlowWrapper*>((void*)s2_info.get_ptr(engine));

	INFO("Tick count s1: " << w1->impl->tick_count);
	INFO("Tick count s2: " << w2->impl->tick_count);

	// We expect 5 ticks issued, 10ms between ticks, and slow-job taking 30ms → can only respond to every 3rd
	CHECK(w1->impl->tick_count == 2);
	CHECK(w2->impl->tick_count == 2);
}

TEST_CASE("Unit/Workloads/SyncedGroupWorkload/tick() passes real time_delta (child thread measures time elapsed since last actionable tick)")
{
	using namespace std::chrono;

	const TickInfo tick_info = TICK_INFO_FIRST_10MS_100HZ;
	const double tick_rate_hz = 1.0 / tick_info.delta_time;

	Model model;
	const auto h = model.add("CountingWorkload", "ticky", tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {h}, tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	const auto& child_info = EngineInspector::get_instance_info(engine, h.index);
	auto* counting = static_cast<CountingWrapper*>((void*)child_info.get_ptr(engine));

	auto* group_ptr = group_info.get_ptr(engine);

	group_info.type->start_fn(group_ptr, tick_rate_hz);

	std::this_thread::sleep_for(20ms);
	group_info.type->tick_fn(group_ptr, tick_info);

	std::this_thread::sleep_for(40ms);
	const double first_dt = counting->impl->last_dt;

	group_info.type->tick_fn(group_ptr, tick_info);

	std::this_thread::sleep_for(10ms); // give a bit if time for the tick to complete
	group_info.type->stop_fn(group_ptr);

	INFO("First time_delta (expected 0.02sec): " << first_dt);
	CHECK_THAT(first_dt, Catch::Matchers::WithinAbs(0.02, 0.005)); // allow ±5ms - since we're not allowing for code-duration when sleeping above

	INFO("Last time_delta (expected 0.04sec): " << counting->impl->last_dt);
	CHECK_THAT(counting->impl->last_dt, Catch::Matchers::WithinAbs(0.04, 0.005)); // (ditto)
}

TEST_CASE("Unit/Workloads/SyncedGroupWorkload/Child allowed to run at slower fixed tick rate than group")
{
	using namespace std::chrono;

	const TickInfo group_tick_info = TICK_INFO_FIRST_10MS_100HZ;
	const double group_tick_rate_hz = 1.0 / group_tick_info.delta_time;
	const double child_tick_rate_hz = 1.0 / TICK_INFO_FIRST_100MS_10HZ.delta_time; // child wants to tick 10x slower than group - we should let it

	Model model;
	const auto h = model.add("CountingWorkload", "slower", child_tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {h}, group_tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	const auto& child_info = EngineInspector::get_instance_info(engine, h.index);
	auto* counting = static_cast<CountingWrapper*>((void*)child_info.get_ptr(engine));
	auto* group_ptr = group_info.get_ptr(engine);

	group_info.type->start_fn(group_ptr, group_tick_rate_hz);

	constexpr int num_group_ticks = 10; // total time: 100ms
	for (int i = 0; i < num_group_ticks; ++i)
	{
		group_info.type->tick_fn(group_ptr, group_tick_info);
		std::this_thread::sleep_for(10ms);
	}

	std::this_thread::sleep_for(20ms); // allow child time to complete final tick
	group_info.type->stop_fn(group_ptr);

	INFO("Child tick count (expected ~2): " << counting->impl->tick_count);
	CHECK(counting->impl->tick_count == 2);
}
