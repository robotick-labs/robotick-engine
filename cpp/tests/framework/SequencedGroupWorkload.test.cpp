// Copyright 2025 Robotick Labs CIC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
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
		void tick(double) { ++tick_count; }
		static void reset() { tick_count = 0; }
	};

	struct DummyTickingWrapper
	{
		DummyTickingWorkload* impl = new DummyTickingWorkload();
		~DummyTickingWrapper() { delete impl; }
		void tick(double dt) { impl->tick(dt); }
	};

	struct DummyTickingRegister
	{
		DummyTickingRegister()
		{
			static const WorkloadRegistryEntry entry = {"DummyTickingWorkload", sizeof(DummyTickingWrapper), alignof(DummyTickingWrapper),
				[](void* p)
				{
					new (p) DummyTickingWrapper();
				},
				[](void* p)
				{
					static_cast<DummyTickingWrapper*>(p)->~DummyTickingWrapper();
				},
				nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, double dt)
				{
					static_cast<DummyTickingWrapper*>(p)->tick(dt);
				},
				nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static DummyTickingRegister s_register_dummy;

	// === SlowTickWorkload ===

	struct SlowTickWorkload
	{
		void tick(double) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
	};

	struct SlowWrapper
	{
		SlowTickWorkload impl;
		void tick(double dt) { impl.tick(dt); }
	};

	struct SlowTickRegister
	{
		SlowTickRegister()
		{
			static const WorkloadRegistryEntry entry = {"SlowTickWorkload", sizeof(SlowWrapper), alignof(SlowWrapper),
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
	static SlowTickRegister s_register_slow;
} // namespace

TEST_CASE("Unit|Workloads|SequencedGroupWorkload|Child ticks are invoked in sequence")
{
	DummyTickingWorkload::reset();

	Model model;
	const auto child1 = model.add("DummyTickingWorkload", "child1", 50.0);
	const auto child2 = model.add("DummyTickingWorkload", "child2", 50.0);
	const auto group = model.add("SequencedGroupWorkload", "group", {child1, child2}, 50.0);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	REQUIRE(group_info.ptr != nullptr);

	REQUIRE_NOTHROW(group_info.type->start_fn(group_info.ptr, 50.0));
	REQUIRE_NOTHROW(group_info.type->tick_fn(group_info.ptr, 0.01));
	REQUIRE_NOTHROW(group_info.type->stop_fn(group_info.ptr));

	CHECK(DummyTickingWorkload::tick_count == 2);
}

TEST_CASE("Unit|Workloads|SequencedGroupWorkload|Overrun logs if exceeded")
{
	Model model;
	const auto handle = model.add("SlowTickWorkload", "slow", 50.0);
	const auto group = model.add("SequencedGroupWorkload", "group", {handle}, 1000.0);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& group_info = EngineInspector::get_instance_info(engine, group.index);
	REQUIRE_NOTHROW(group_info.type->tick_fn(group_info.ptr, 0.001)); // 1ms budget, expect warning log
}
