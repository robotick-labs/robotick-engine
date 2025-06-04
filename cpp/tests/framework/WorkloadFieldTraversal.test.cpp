
// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/TypeId.h"
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
		ROBOTICK_FIELD(SimpleInputs, int, input_value)
		ROBOTICK_END_FIELDS()

		struct SimpleOutputs
		{
			int output_value = 0;
		};

		ROBOTICK_BEGIN_FIELDS(SimpleOutputs)
		ROBOTICK_FIELD(SimpleOutputs, int, output_value)
		ROBOTICK_END_FIELDS()

		struct SimpleWorkload
		{
			SimpleInputs inputs;
			SimpleOutputs outputs;

			void tick(const TickInfo&) { outputs.output_value = inputs.input_value + 1; }
		};

		ROBOTICK_DEFINE_WORKLOAD(SimpleWorkload, void, SimpleInputs, SimpleOutputs)
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
		auto* workload_ptr = static_cast<SimpleWorkload*>((void*)workload_infos[0].get_ptr(engine));
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
				CHECK(workloads_buf.contains_object(field_ptr, view.field->size));

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

				CHECK(mirror_buf.contains_object(field_ptr, view.field->size));

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

	TEST_CASE("Unit|Framework|FieldWalker|for_each_workload returns all instances")
	{
		Model model;
		auto w1 = model.add("SimpleWorkload", "W1", 10.0);
		model.add("SimpleWorkload", "W2", 10.0);
		model.set_root(w1);

		Engine engine;
		engine.load(model);

		std::vector<std::string> seen_names;
		WorkloadFieldsIterator::for_each_workload(engine,
			[&](const WorkloadInstanceInfo& info)
			{
				seen_names.push_back(std::string(info.unique_name.c_str()));
			});

		REQUIRE(seen_names.size() == 2);
		CHECK_THAT(seen_names, Catch::Matchers::Contains("W1"));
		CHECK_THAT(seen_names, Catch::Matchers::Contains("W2"));
	}

	TEST_CASE("Unit|Framework|FieldWalker|for_each_field_in_workload walks individual workload fields")
	{
		Model model;
		auto w = model.add("SimpleWorkload", "Solo", 10.0);
		model.set_root(w);

		Engine engine;
		engine.load(model);

		const auto& info = EngineInspector::get_all_instance_info(engine)[0];

		std::set<std::string> found_fields;
		WorkloadFieldsIterator::for_each_field_in_workload(engine, info, nullptr,
			[&](const WorkloadFieldView& view)
			{
				found_fields.insert(view.field->name.c_str());
			});

		CHECK(found_fields.count("input_value") == 1);
		CHECK(found_fields.count("output_value") == 1);
	}

	TEST_CASE("Unit|Framework|FieldWalker|Blackboard subfields correctly walked via input-wrapped Blackboard")
	{
		struct BBInputs
		{
			Blackboard blackboard;

			BBInputs() : blackboard({BlackboardFieldInfo("x", TypeId(GET_TYPE_ID(int))), BlackboardFieldInfo("y", TypeId(GET_TYPE_ID(double)))}) {}
		};
		ROBOTICK_BEGIN_FIELDS(BBInputs)
		ROBOTICK_FIELD(BBInputs, Blackboard, blackboard)
		ROBOTICK_END_FIELDS()

		struct BBWorkload
		{
			BBInputs inputs;
			void tick(const TickInfo&) {}
		};
		ROBOTICK_DEFINE_WORKLOAD(BBWorkload, void, BBInputs, void)

		Model model;
		auto handle = model.add("BBWorkload", "BB", 10.0);
		model.set_root(handle);

		Engine engine;
		engine.load(model);

		std::set<std::string> seen_fields;
		WorkloadFieldsIterator::for_each_workload_field(engine, nullptr,
			[&](const WorkloadFieldView& view)
			{
				if (view.subfield)
					seen_fields.insert(view.subfield->name.c_str());
			});

		CHECK(seen_fields.count("x") == 1);
		CHECK(seen_fields.count("y") == 1);
	}

} // namespace robotick::test
