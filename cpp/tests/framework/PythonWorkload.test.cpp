// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "utils/EngineInspector.h"

#include <catch2/catch_all.hpp>

using namespace robotick;
using namespace robotick::test;

TEST_CASE("Unit|Workloads|PythonWorkload|Metadata is registered correctly")
{
	const auto* cfg = FieldRegistry::get().get_struct("PythonConfig");
	const auto* in = FieldRegistry::get().get_struct("PythonInputs");
	const auto* out = FieldRegistry::get().get_struct("PythonOutputs");

	REQUIRE(cfg != nullptr);
	REQUIRE(in != nullptr);
	REQUIRE(out != nullptr);

	REQUIRE(cfg->fields.size() == 2);
	REQUIRE(in->fields.size() == 1);
	REQUIRE(out->fields.size() == 1);
}

TEST_CASE("Unit|Workloads|PythonWorkload|Python tick executes")
{
	Model model;
	const auto handle = model.add(
		"PythonWorkload", "test2", 1.0, {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});
	model.set_root(handle);

	Engine engine;
	engine.load(model);
	const auto& info = EngineInspector::get_instance_info(engine, handle.index);

	REQUIRE(info.ptr != nullptr);
	REQUIRE(info.type != nullptr);
	REQUIRE(info.type->tick_fn != nullptr);

	REQUIRE_NOTHROW(info.type->tick_fn(info.ptr, 0.01));
}

TEST_CASE("Unit|Workloads|PythonWorkload|Output reflects Python computation")
{
	Model model;
	const auto handle = model.add("PythonWorkload", "py", 1.0,
		{{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}, {"example_in", 21.0}});
	model.set_root(handle);

	Engine engine;
	engine.load(model);

	const auto& info = EngineInspector::get_instance_info(engine, handle.index);
	REQUIRE(info.ptr != nullptr);
	REQUIRE(info.type != nullptr);
	REQUIRE(info.type->tick_fn != nullptr);

	// Execute tick
	info.type->tick_fn(info.ptr, 0.01);

	// Reflect over outputs
	const auto* output_struct = info.type->output_struct;
	REQUIRE(output_struct != nullptr);

#if defined(PYTHON_BLACKBOARDS_SUPPORTED)
	const void* output_base = static_cast<const uint8_t*>(info.ptr) + info.type->output_offset;

	bool found = false;
	for (const auto& field : output_struct->fields)
	{
		if (field.name == "greeting")
		{
			const void* field_ptr = static_cast<const uint8_t*>(output_base) + field.offset;
			double value = *static_cast<const double*>(field_ptr);
			REQUIRE(value == Catch::Approx(42.0));
			found = true;
			break;
		}
	}

	REQUIRE(found);
#endif // #ifdef PYTHON_BLACKBOARDS_SUPPORTED
}

TEST_CASE("Unit|Workloads|PythonWorkload|start/stop hooks are optional and safe")
{
	Model model;
	const auto handle = model.add(
		"PythonWorkload", "test", 10.0, {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});
	model.set_root(handle);

	Engine engine;
	engine.load(model);

	const auto& info = EngineInspector::get_instance_info(engine, handle.index);
	REQUIRE(info.ptr != nullptr);
	REQUIRE(info.type != nullptr);

	if (info.type->start_fn)
	{
		REQUIRE_NOTHROW(info.type->start_fn(info.ptr, 10.0));
	}
	if (info.type->stop_fn)
	{
		REQUIRE_NOTHROW(info.type->stop_fn(info.ptr));
	}
}
