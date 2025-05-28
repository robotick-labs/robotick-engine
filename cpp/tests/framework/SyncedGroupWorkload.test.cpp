// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
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

		void tick(double dt)
		{
			last_dt = dt;
			tick_count.fetch_add(1);
		}
	};

	struct CountingWrapper
	{
		CountingWorkload* impl = new CountingWorkload();
		~CountingWrapper() { delete impl; }
		void tick(double dt) { impl->tick(dt); }
	};

	struct CountingRegister
	{
		CountingRegister()
		{
			const WorkloadRegistryEntry entry = {"CountingWorkload", sizeof(CountingWrapper), alignof(CountingWrapper),
				[](void* p)
				{
					new (p) CountingWrapper();
				},
				[](void* p)
				{
					static_cast<CountingWrapper*>(p)->~CountingWrapper();
				},
				nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, double dt)
				{
					static_cast<CountingWrapper*>(p)->tick(dt);
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
		void tick(double)
		{
			tick_count++;
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	};

	struct SlowWrapper
	{
		SlowWorkload* impl = new SlowWorkload();
		~SlowWrapper() { delete impl; }
		void tick(double dt) { impl->tick(dt); }
	};

	struct SlowRegister
	{
		SlowRegister()
		{
			const WorkloadRegistryEntry entry = {"SlowWorkload", sizeof(SlowWrapper), alignof(SlowWrapper),
				[](void* p)
				{
					new (p) SlowWrapper();
				},
				[](void* p)
				{
					static_cast<SlowWrapper*>(p)->~SlowWrapper();
				},
				nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, double dt)
				{
					static_cast<SlowWrapper*>(p)->tick(dt);
				},
				nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static SlowRegister s_register_slow;
} // namespace

TEST_CASE("Unit|Workloads|SyncedGroupWorkload|All children tick in parallel")
{
	const double tick_rate_hz = 100.0;
	const int tick_count = 5;
	const double tick_interval = 1.0 / tick_rate_hz;

	Model model;
	const auto a = model.add("CountingWorkload", "a", tick_rate_hz);
	const auto b = model.add("CountingWorkload", "b", tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {a, b}, tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& info = EngineInspector::get_instance_info(engine, group.index);
	REQUIRE(info.ptr != nullptr);

	info.type->start_fn(info.ptr, tick_rate_hz);

	const auto& child_a = EngineInspector::get_instance_info(engine, a.index);
	const auto& child_b = EngineInspector::get_instance_info(engine, b.index);
	auto* wa = static_cast<CountingWrapper*>((void*)child_a.ptr);
	auto* wb = static_cast<CountingWrapper*>((void*)child_b.ptr);

	for (int i = 0; i < tick_count; ++i)
	{
		info.type->tick_fn(info.ptr, tick_interval);

		std::this_thread::sleep_for(std::chrono::duration<double>(tick_interval));

		// Confirm each child has exactly i + 1 ticks after this iteration
		CHECK(wa->impl->tick_count == i + 1);
		CHECK(wb->impl->tick_count == i + 1);
	}

	info.type->stop_fn(info.ptr);
}

TEST_CASE("Unit|Workloads|SyncedGroupWorkload|Child busy flags skip ticks")
{
	using namespace std::chrono;

	constexpr double tick_rate_hz = 100.0;
	constexpr auto tick_interval = duration<double>(1.0 / tick_rate_hz);
	constexpr int num_ticks = 5;

	Model model;
	const auto s1 = model.add("SlowWorkload", "s1", tick_rate_hz);
	const auto s2 = model.add("SlowWorkload", "s2", tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {s1, s2}, tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	REQUIRE(group_info.ptr != nullptr);

	group_info.type->start_fn(group_info.ptr, tick_rate_hz);

	for (int i = 0; i < num_ticks; ++i)
	{
		group_info.type->tick_fn(group_info.ptr, tick_interval.count());
		std::this_thread::sleep_for(10ms); // Let threads get through, but not enough for all to finish
	}

	group_info.type->stop_fn(group_info.ptr);

	const auto& s1_info = EngineInspector::get_instance_info(engine, s1.index);
	const auto& s2_info = EngineInspector::get_instance_info(engine, s2.index);
	const auto* w1 = static_cast<SlowWrapper*>((void*)s1_info.ptr);
	const auto* w2 = static_cast<SlowWrapper*>((void*)s2_info.ptr);

	INFO("Tick count s1: " << w1->impl->tick_count);
	INFO("Tick count s2: " << w2->impl->tick_count);

	// We expect 5 ticks issued, 10ms between ticks, and slow-job taking 30ms → can only respond to every 3rd
	CHECK(w1->impl->tick_count == 2);
	CHECK(w2->impl->tick_count == 2);
}

TEST_CASE("Unit|Workloads|SyncedGroupWorkload|tick() passes real time_delta (child thread measures time elapsed since last actionable tick)")
{
	using namespace std::chrono;

	constexpr double tick_rate_hz = 10.0;
	constexpr auto tick_interval = duration<double>(1.0 / tick_rate_hz);

	Model model;
	const auto h = model.add("CountingWorkload", "ticky", tick_rate_hz);
	const auto group = model.add("SyncedGroupWorkload", "group", {h}, tick_rate_hz);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	const auto& child_info = EngineInspector::get_instance_info(engine, h.index);
	auto* counting = static_cast<CountingWrapper*>((void*)child_info.ptr);

	group_info.type->start_fn(group_info.ptr, tick_rate_hz);

	std::this_thread::sleep_for(20ms);
	group_info.type->tick_fn(group_info.ptr, tick_interval.count());

	std::this_thread::sleep_for(30ms);
	const double first_dt = counting->impl->last_dt;

	group_info.type->tick_fn(group_info.ptr, tick_interval.count());

	std::this_thread::sleep_for(10ms); // give a bit if time for the tick to complete
	group_info.type->stop_fn(group_info.ptr);

	INFO("First dt (expected 0.02): " << first_dt);
	CHECK_THAT(first_dt, Catch::Matchers::WithinAbs(0.02, 0.005)); // allow ±5ms - since we're not allowing for code-duration when sleeping above

	INFO("Last dt (expected 0.03): " << counting->impl->last_dt);
	CHECK_THAT(counting->impl->last_dt, Catch::Matchers::WithinAbs(0.02, 0.005)); // (ditto)
}
