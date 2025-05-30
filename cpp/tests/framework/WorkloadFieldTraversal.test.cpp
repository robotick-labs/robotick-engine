
// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"
#include "utils/EngineInspector.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		struct SimpleInputs
		{
			int input_value = 0;
		};

		ROBOTICK_BEGIN_FIELDS(SimpleInputs)
		ROBOTICK_FIELD(SimpleInputs, input_value)
		ROBOTICK_END_FIELDS()

		struct SimpleOutputs
		{
			int output_value = 0;
		};

		ROBOTICK_BEGIN_FIELDS(SimpleOutputs)
		ROBOTICK_FIELD(SimpleOutputs, output_value)
		ROBOTICK_END_FIELDS()

		struct SimpleWorkload
		{
			SimpleInputs inputs;
			SimpleOutputs outputs;

			void tick(double) { outputs.output_value = inputs.input_value + 1; }
		};

		ROBOTICK_DEFINE_WORKLOAD(SimpleWorkload)
	} // namespace

	TEST_CASE("Unit|Framework|FieldWalker|Field pointers match expected values and buffers")
	{
		Model model;
		auto w = model.add("SimpleWorkload", "W", 10.0);
		model.set_root(w);

		Engine engine;
		engine.load(model);

		const auto& workload_infos = EngineInspector::get_all_instance_info(engine);
		REQUIRE(workload_infos.size() == 1);

		const auto& workloads_buf = EngineInspector::get_workloads_buffer(engine);

		// Set known values in workload memory
		auto* workload_ptr = reinterpret_cast<SimpleWorkload*>(workload_infos[0].get_ptr(engine));
		workload_ptr->inputs.input_value = 42;
		workload_ptr->outputs.output_value = 123;

		bool found_input = false;
		bool found_output = false;

		WorkloadFieldsIterator::for_each_workload_field(engine, nullptr,
			[&](const WorkloadFieldView& view)
			{
				CHECK(view.instance->unique_name == "W");
				const auto* field_ptr = static_cast<const uint8_t*>(view.field_ptr);

				// Verify pointer lies within workloads buffer
				CHECK(workloads_buf.is_within_buffer(field_ptr));

				if (view.field->name == "input_value")
				{
					found_input = true;
					CHECK(*static_cast<const int*>(view.field_ptr) == 42);
				}
				if (view.field->name == "output_value")
				{
					found_output = true;
					CHECK(*static_cast<const int*>(view.field_ptr) == 123);
				}
			});

		CHECK(found_input);
		CHECK(found_output);
	}

	TEST_CASE("Unit|Framework|FieldWalker|Override buffer works correctly")
	{
		Model model;
		auto w = model.add("SimpleWorkload", "W", 10.0);
		model.set_root(w);

		Engine engine;
		engine.load(model);

		// const auto& original = EngineInspector::get_all_instance_info(engine);
		const auto& original_buf = EngineInspector::get_workloads_buffer(engine);

		WorkloadsBuffer mirror_buf(original_buf.get_size());
		std::memcpy(mirror_buf.raw_ptr(), original_buf.raw_ptr(), original_buf.get_size());

		auto* mirror_workload = reinterpret_cast<SimpleWorkload*>(mirror_buf.raw_ptr());
		mirror_workload->inputs.input_value = 99;
		mirror_workload->outputs.output_value = 888;

		bool found_input = false;
		bool found_output = false;

		WorkloadFieldsIterator::for_each_workload_field(engine, &mirror_buf,
			[&](const WorkloadFieldView& view)
			{
				CHECK(view.instance->unique_name == "W");
				const auto* field_ptr = static_cast<const uint8_t*>(view.field_ptr);

				CHECK(mirror_buf.is_within_buffer(field_ptr));

				if (view.field->name == "input_value")
				{
					found_input = true;
					CHECK(*static_cast<const int*>(view.field_ptr) == 99);
				}
				if (view.field->name == "output_value")
				{
					found_output = true;
					CHECK(*static_cast<const int*>(view.field_ptr) == 888);
				}
			});

		CHECK(found_input);
		CHECK(found_output);
	}
} // namespace robotick::test
