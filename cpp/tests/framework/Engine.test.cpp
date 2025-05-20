#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "utils/EngineInspector.h"
#include "utils/ModelHelper.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

using namespace Catch;
using namespace robotick;
using namespace robotick::test;

namespace
{
	// === DummyWorkload (with config/load) ===

	struct DummyConfig
	{
		int value = 0;
		ROBOTICK_DECLARE_FIELDS();
	};
	ROBOTICK_DEFINE_FIELDS(DummyConfig, ROBOTICK_FIELD(DummyConfig, value))

	struct DummyWorkload
	{
		DummyConfig config;
		int loaded_value = 0;

		void load() { loaded_value = config.value; }
	};

	struct DummyWorkloadRegister
	{
		DummyWorkloadRegister()
		{
			static const WorkloadRegistryEntry entry = {"DummyWorkload", sizeof(DummyWorkload), alignof(DummyWorkload),
				[](void* p)
				{
					new (p) DummyWorkload();
				},
				[](void* p)
				{
					static_cast<DummyWorkload*>(p)->~DummyWorkload();
				},
				DummyConfig::get_struct_reflection(), offsetof(DummyWorkload, config), nullptr, 0, nullptr, 0, nullptr, nullptr,
				[](void* p)
				{
					static_cast<DummyWorkload*>(p)->load();
				},
				nullptr, nullptr, nullptr, nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static DummyWorkloadRegister s_register_dummy;

	// === TickCounterWorkload ===

	struct TickCounterWorkload
	{
		int count = 0;
		void tick(double) { count++; }
	};

	struct TickCounterWorkloadRegister
	{
		TickCounterWorkloadRegister()
		{
			static const WorkloadRegistryEntry entry = {"TickCounterWorkload", sizeof(TickCounterWorkload), alignof(TickCounterWorkload),
				[](void* p)
				{
					new (p) TickCounterWorkload();
				},
				[](void* p)
				{
					static_cast<TickCounterWorkload*>(p)->~TickCounterWorkload();
				},
				nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
				[](void* p, double dt)
				{
					static_cast<TickCounterWorkload*>(p)->tick(dt);
				},
				nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static TickCounterWorkloadRegister s_register_tick;
} // namespace

// === Tests ===

TEST_CASE("Unit|Framework|Engine|DummyWorkload stores tick rate correctly")
{
	Model model;
	auto handle = model.add("DummyWorkload", "A", 123.0, {});
	model.set_root(handle);

	Engine engine;
	engine.load(model);

	double tick_rate = EngineInspector::get_instance_info(engine, 0).tick_rate_hz;
	REQUIRE(tick_rate == 123.0);
}

TEST_CASE("Unit|Framework|Engine|DummyWorkload config is loaded via load()")
{
	Model model;
	auto handle = model.add("DummyWorkload", "A", 1.0, {{"value", 42}});
	model.set_root(handle);

	Engine engine;
	engine.load(model);

	const DummyWorkload* ptr = EngineInspector::get_instance<DummyWorkload>(engine, 0);
	REQUIRE(ptr->loaded_value == 42);
}

TEST_CASE("Unit|Framework|Engine|Rejects unknown workload type")
{
	Model model;
	auto handle = model.add("UnknownType", "fail", 1.0, {});
	model.set_root(handle);

	Engine engine;
	REQUIRE_THROWS_WITH(engine.load(model), "Unknown workload type: UnknownType");
}

TEST_CASE("Unit|Framework|Engine|Multiple workloads supported")
{
	Model model;
	model.add("DummyWorkload", "one", 1.0, {{"value", 1}});
	model.add("DummyWorkload", "two", 2.0, {{"value", 2}});
	model_helpers::wrap_all_in_sequenced_group(model);

	Engine engine;
	engine.load(model);

	const DummyWorkload* one = EngineInspector::get_instance<DummyWorkload>(engine, 0);
	const DummyWorkload* two = EngineInspector::get_instance<DummyWorkload>(engine, 1);

	REQUIRE(one->loaded_value == 1);
	REQUIRE(two->loaded_value == 2);
}

TEST_CASE("Unit|Framework|Engine|Workloads receive tick call")
{
	Model model;
	auto handle = model.add("TickCounterWorkload", "ticky", 200.0, {});
	model.set_root(handle);

	Engine engine;
	engine.load(model);

	std::atomic<bool> stop_flag = true;
	engine.run(stop_flag); // will tick at least once

	const TickCounterWorkload* ptr = EngineInspector::get_instance<TickCounterWorkload>(engine, 0);
	REQUIRE(ptr->count >= 1);
}
