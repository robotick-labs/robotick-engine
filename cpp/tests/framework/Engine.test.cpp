// Engine.test.cpp
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "utils/EngineInspector.h"

#include <any>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <map>
#include <string>
#include <thread>

using namespace Catch;
using namespace robotick;
using namespace robotick::test_access;

namespace
{
	struct DummyConfig
	{
		int value = 0;
		ROBOTICK_DECLARE_FIELDS();
	};

	ROBOTICK_DEFINE_FIELDS(DummyConfig, ROBOTICK_FIELD(DummyConfig, value))

	struct DummyWorkload
	{
		DummyConfig config;
		int			loaded_value = 0;

		void load() { loaded_value = config.value; }
	};

	void register_dummy_type()
	{
		static const WorkloadRegistryEntry entry = {
			"DummyWorkload",
			sizeof(DummyWorkload),
			alignof(DummyWorkload),
			[](void* p) { new (p) DummyWorkload(); },
			[](void* p) { static_cast<DummyWorkload*>(p)->~DummyWorkload(); },
			DummyConfig::get_struct_reflection(),
			offsetof(DummyWorkload, config),
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			nullptr,
			[](void* p) { static_cast<DummyWorkload*>(p)->load(); },
			nullptr,
			nullptr,
			nullptr // <- fix: add stop_fn explicitly
		};

		WorkloadRegistry::get().register_entry(entry);
	}

} // namespace

TEST_CASE("Unit|Framework|Engine|DummyWorkload workload stores tick rate correctly")
{
	register_dummy_type();

	Model model;
	model.add("DummyWorkload", "A", 123.0, {});

	Engine engine;
	engine.load(model);
	engine.setup();

	const double tick_rate_hz = EngineInspector::get_instance_info(engine, 0).tick_rate_hz;
	REQUIRE(tick_rate_hz == 123.0);
}

TEST_CASE("Unit|Framework|Engine|DummyWorkload workload stores config correctly")
{
	register_dummy_type();

	Model model;
	model.add("DummyWorkload", "A", 1.0, {{"value", 42}});

	Engine engine;
	engine.load(model);
	engine.setup();

	const DummyWorkload* ptr = EngineInspector::get_instance<DummyWorkload>(engine, 0);
	REQUIRE(ptr->loaded_value == 42);
}

TEST_CASE("Unit|Framework|Engine|Rejects unknown workload type")
{
	Model model;
	model.add("UnknownType", "fail", 1.0, {});

	Engine engine;
	REQUIRE_THROWS_WITH(engine.load(model), "Unknown workload type: UnknownType");
}

TEST_CASE("Unit|Framework|Engine|Multiple workloads allowed")
{
	register_dummy_type();

	Model model;
	model.add("DummyWorkload", "one", 1.0, {{"value", 1}});
	model.add("DummyWorkload", "two", 2.0, {{"value", 2}});

	Engine engine;
	engine.load(model);
	engine.setup();

	const DummyWorkload* one = EngineInspector::get_instance<DummyWorkload>(engine, 0);
	const DummyWorkload* two = EngineInspector::get_instance<DummyWorkload>(engine, 1);

	REQUIRE(one->loaded_value == 1);
	REQUIRE(two->loaded_value == 2);
}

TEST_CASE("Unit|Framework|Engine|Workloads are ticked")
{
	struct TickCounterWorkload
	{
		int	 count = 0;
		void tick(double) { count++; }
	};

	static const WorkloadRegistryEntry entry = {
		"TickCounterWorkload",
		sizeof(TickCounterWorkload),
		alignof(TickCounterWorkload),
		[](void* p) { new (p) TickCounterWorkload(); },
		[](void* p) { static_cast<TickCounterWorkload*>(p)->~TickCounterWorkload(); },
		nullptr,
		0,
		nullptr,
		0,
		nullptr,
		0,
		[](void* p, double dt) { static_cast<TickCounterWorkload*>(p)->tick(dt); },
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr};

	WorkloadRegistry::get().register_entry(entry);

	Model model;
	model.add("TickCounterWorkload", "ticky", 200.0, {});

	Engine engine;
	engine.load(model);
	engine.setup();
	engine.start();

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	engine.stop();

	const TickCounterWorkload* ptr = EngineInspector::get_instance<TickCounterWorkload>(engine, 0);
	REQUIRE(ptr->count > 1);
}
