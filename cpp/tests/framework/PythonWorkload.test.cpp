// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/utils/TypeId.h"

#include <catch2/catch_all.hpp>

using namespace robotick;
using namespace robotick::test;

#ifndef ROBOTICK_ENABLE_PYTHON
#error "ROBOTICK_ENABLE_PYTHON must be defined (expected value: 1)"
#endif

#if ROBOTICK_ENABLE_PYTHON != 1
#error "ROBOTICK_ENABLE_PYTHON must be set to 1"
#endif

TEST_CASE("Unit/Workloads/PythonWorkload")
{
	SECTION("Python tick executes")
	{
		Model model;
		const WorkloadSeed& python_workload = model.add( "PythonWorkload", "test2").set_tick_rate_hz(1.0f).set_config{{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});
		model.set_root_workload(python_workload);

		Engine engine;
		engine.load(model);
		const auto& info = engine.find_instance_info(python_workload.);
		auto* inst_ptr = info.get_ptr(engine);

		REQUIRE(inst_ptr);
		REQUIRE(info.type != nullptr);
		REQUIRE(info.type->tick_fn != nullptr);

		REQUIRE_NOTHROW(info.type->tick_fn(inst_ptr, TICK_INFO_FIRST_10MS_100HZ));
	}

	SECTION("Output reflects Python computation")
	{
		Model model;
		const WorkloadSeed& root =
			model.add("PythonWorkload", "py")
				.set_tick_rate_hz(1.0f)
				.set_config(
					{{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}, {"example_in", "21.0"}});
		model.set_root_workload(root);

		Engine engine;
		engine.load(model);

		const auto& info = engine.find_instance_info(root.unique_name);
		auto* inst_ptr = info.get_ptr(engine);
		REQUIRE(inst_ptr != nullptr);
		REQUIRE(info.type != nullptr);
		REQUIRE(info.type->tick_fn != nullptr);

		// Execute tick
		info.type->tick_fn(inst_ptr, TICK_INFO_FIRST_10MS_100HZ);

		// === Find the output blackboard ===
		const auto* output_struct = info.type->output_struct;
		REQUIRE(output_struct != nullptr);

		const void* output_base = static_cast<const uint8_t*>(inst_ptr) + output_struct->offset_within_workload;

		const robotick::Blackboard* output_blackboard = nullptr;
		for (const auto& field : output_struct->fields)
		{
			if (field.name == "blackboard")
			{
				ROBOTICK_ASSERT(field.offset_within_struct != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

				const void* field_ptr = static_cast<const uint8_t*>(output_base) + field.offset_within_struct;
				output_blackboard = static_cast<const robotick::Blackboard*>(field_ptr);
				break;
			}
		}

		REQUIRE(output_blackboard->has("greeting"));
		const FixedString64 greeting = output_blackboard->get<FixedString64>("greeting");
		REQUIRE(std::string(greeting).substr(0, 15) == "[Python] Hello!");

		REQUIRE(output_blackboard->has("val_double"));
		const double val_double = output_blackboard->get<double>("val_double");
		REQUIRE(val_double == 1.23);

		REQUIRE(output_blackboard->has("val_int"));
		const int val_int = output_blackboard->get<int>("val_int");
		REQUIRE(val_int == 456);
	}

	SECTION("start/stop hooks are optional and safe")
	{
		Model model;
		const WorkloadSeed& root =
			model.add("PythonWorkload", "test")
				.set_tick_rate_hz(10.0f)
				.set_config({{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});
		model.set_root_workload(root);

		Engine engine;
		engine.load(model);

		const auto& info = engine.find_instance_info(root.unique_name);
		auto* inst_ptr = info.get_ptr(engine);

		REQUIRE(inst_ptr != nullptr);
		REQUIRE(info.type != nullptr);

		if (info.type->start_fn)
		{
			REQUIRE_NOTHROW(info.type->start_fn(inst_ptr, 10.0));
		}
		if (info.type->stop_fn)
		{
			REQUIRE_NOTHROW(info.type->stop_fn(inst_ptr));
		}
	}
}
