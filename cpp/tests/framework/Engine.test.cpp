// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/platform/Threading.h"

#include "utils/EngineInspector.h"
#include "utils/ModelHelper.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

namespace robotick::test
{

	namespace
	{
		// === DummyWorkload (with config/load) ===

		struct DummyConfig
		{
			int value = 0;
		};
		ROBOTICK_BEGIN_FIELDS(DummyConfig)
		ROBOTICK_FIELD(DummyConfig, int, value)
		ROBOTICK_END_FIELDS()

		struct DummyWorkload
		{
			DummyConfig config;
			int loaded_value = 0;

			void load() { loaded_value = config.value; }
		};
		ROBOTICK_DEFINE_WORKLOAD(DummyWorkload, DummyConfig)

		// === TickCounterWorkload ===

		struct TickCounterWorkload
		{
			int count = 0;
			void tick(double) { count++; }
		};
		ROBOTICK_DEFINE_WORKLOAD(TickCounterWorkload)

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
		REQUIRE(tick_rate == Catch::Approx(123.0));
	}

	TEST_CASE("Unit|Framework|Engine|DummyWorkload config is loaded via load()")
	{
		Model model;
		auto handle = model.add("DummyWorkload", "A", 1.0, {{"value", "42"}});
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
		REQUIRE_THROWS(engine.load(model));
	}

	TEST_CASE("Unit|Framework|Engine|Multiple workloads supported")
	{
		Model model;
		model.add("DummyWorkload", "one", 1.0, {{"value", "1"}});
		model.add("DummyWorkload", "two", 2.0, {{"value", "2"}});
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

		AtomicFlag stop_after_next_tick_flag{true};
		engine.run(stop_after_next_tick_flag); // will tick at least once even if stop_after_next_tick_flag is true

		const TickCounterWorkload* ptr = EngineInspector::get_instance<TickCounterWorkload>(engine, 0);
		REQUIRE(ptr->count >= 1);
	}

} // namespace robotick::test
