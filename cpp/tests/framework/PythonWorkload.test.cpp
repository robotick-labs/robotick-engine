// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
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

	REQUIRE(cfg->fields.size() == 3);
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

	// === Find the output blackboard ===
	const auto* output_struct = info.type->output_struct;
	REQUIRE(output_struct != nullptr);

	const void* output_base = static_cast<const uint8_t*>(info.ptr) + output_struct->offset;

	const robotick::Blackboard* output_blackboard = nullptr;
	for (const auto& field : output_struct->fields)
	{
		if (field.name == "blackboard")
		{
			const void* field_ptr = static_cast<const uint8_t*>(output_base) + field.offset;
			output_blackboard = static_cast<const robotick::Blackboard*>(field_ptr);
			break;
		}
	}

	REQUIRE(output_blackboard->has("greeting"));
	const FixedString64 greeting = output_blackboard->get<FixedString64>("greeting");
	REQUIRE(greeting.to_string().substr(0, 15) == "[Python] Hello!");

	REQUIRE(output_blackboard->has("val_double"));
	const double val_double = output_blackboard->get<double>("val_double");
	REQUIRE(val_double == 1.23);

	REQUIRE(output_blackboard->has("val_int"));
	const int val_int = output_blackboard->get<int>("val_int");
	REQUIRE(val_int == 456);
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
