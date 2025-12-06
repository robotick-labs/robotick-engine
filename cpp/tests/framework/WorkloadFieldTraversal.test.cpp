// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/strings/StringUtils.h"
#include "robotick/framework/utils/TypeId.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		struct SimpleInputs
		{
			int input_value = 0;
		};

		ROBOTICK_REGISTER_STRUCT_BEGIN(SimpleInputs)
		ROBOTICK_STRUCT_FIELD(SimpleInputs, int, input_value)
		ROBOTICK_REGISTER_STRUCT_END(SimpleInputs)

		struct SimpleOutputs
		{
			int output_value = 0;
		};

		ROBOTICK_REGISTER_STRUCT_BEGIN(SimpleOutputs)
		ROBOTICK_STRUCT_FIELD(SimpleOutputs, int, output_value)
		ROBOTICK_REGISTER_STRUCT_END(SimpleOutputs)

		struct SimpleWorkload
		{
			SimpleInputs inputs;
			SimpleOutputs outputs;

			void tick(const TickInfo&) { outputs.output_value = inputs.input_value + 1; }
		};

		ROBOTICK_REGISTER_WORKLOAD(SimpleWorkload, void, SimpleInputs, SimpleOutputs)
	} // namespace

	TEST_CASE("Unit/Framework/WorkloadFieldsIterator")
	{
		SECTION("Field pointers match expected values and buffers")
		{
			Model model;
			const WorkloadSeed& w = model.add("SimpleWorkload", "W").set_tick_rate_hz(10.0f);
			model.set_root_workload(w);

			Engine engine;
			engine.load(model);

			const auto& workload_infos = engine.get_all_instance_info();
			REQUIRE(workload_infos.size() == 1);

			const auto& workloads_buf = engine.get_workloads_buffer();

			// Set known values in workload memory
			auto* workload_ptr = static_cast<SimpleWorkload*>((void*)workload_infos[0].get_ptr(engine));
			workload_ptr->inputs.input_value = 42;
			workload_ptr->outputs.output_value = 123;

			bool found_input = false;
			bool found_output = false;

			WorkloadFieldsIterator::for_each_workload_field(engine,
				nullptr,
				[&](const WorkloadFieldView& view)
				{
					CHECK(view.workload_info->seed->unique_name == "W");
					const auto* field_ptr = static_cast<const uint8_t*>(view.field_ptr);

					// Verify pointer lies within workloads buffer
					CHECK(workloads_buf.contains_object_used_space(field_ptr, view.field_info->find_type_descriptor()->size));

					if (view.field_info->name == "input_value")
					{
						found_input = true;
						CHECK(*static_cast<const int*>(view.field_ptr) == 42);
					}
					if (view.field_info->name == "output_value")
					{
						found_output = true;
						CHECK(*static_cast<const int*>(view.field_ptr) == 123);
					}
				});

			CHECK(found_input);
			CHECK(found_output);
		}

		SECTION("Override buffer works correctly")
		{
			Model model;
			const WorkloadSeed& w = model.add("SimpleWorkload", "W").set_tick_rate_hz(10.0f);
			model.set_root_workload(w);

			Engine engine;
			engine.load(model);

			// const auto& original = engine.get_all_instance_info();
			const auto& original_buf = engine.get_workloads_buffer();

			WorkloadsBuffer mirror_buf(original_buf.get_size_used());
			::memcpy(mirror_buf.raw_ptr(), original_buf.raw_ptr(), original_buf.get_size_used());

			const auto& inst = engine.get_all_instance_info()[0];
			auto* mirror_workload = reinterpret_cast<SimpleWorkload*>(mirror_buf.raw_ptr() + inst.offset_in_workloads_buffer);

			mirror_workload->inputs.input_value = 99;
			mirror_workload->outputs.output_value = 888;

			bool found_input = false;
			bool found_output = false;

			WorkloadFieldsIterator::for_each_workload_field(engine,
				&mirror_buf,
				[&](const WorkloadFieldView& view)
				{
					CHECK(view.workload_info->seed->unique_name == "W");
					const auto* field_ptr = static_cast<const uint8_t*>(view.field_ptr);

					CHECK(mirror_buf.contains_object(field_ptr, view.field_info->find_type_descriptor()->size));

					if (view.field_info->name == "input_value")
					{
						found_input = true;
						CHECK(*static_cast<const int*>(view.field_ptr) == 99);
					}
					if (view.field_info->name == "output_value")
					{
						found_output = true;
						CHECK(*static_cast<const int*>(view.field_ptr) == 888);
					}
				});

			CHECK(found_input);
			CHECK(found_output);
		}

		SECTION("for_each_workload returns all instances")
		{
			Model model;
			const WorkloadSeed& w1 = model.add("SimpleWorkload", "W1").set_tick_rate_hz(10.0f);
			model.add("SimpleWorkload", "W2").set_tick_rate_hz(10.0f);
			model.set_root_workload(w1);

			Engine engine;
			engine.load(model);

			FixedVector<FixedString64, 16> seen_names;
			WorkloadFieldsIterator::for_each_workload(engine,
				[&](const WorkloadInstanceInfo& info)
				{
					FixedString64 copied_name(info.seed->unique_name.c_str());
					seen_names.add(copied_name);
				});

			REQUIRE(seen_names.size() == 2);
			bool has_w1 = false;
			bool has_w2 = false;
			for (const auto& name : seen_names)
			{
				if (string_equals(name.c_str(), "W1"))
					has_w1 = true;
				if (string_equals(name.c_str(), "W2"))
					has_w2 = true;
			}
			CHECK(has_w1);
			CHECK(has_w2);
		}

		SECTION("for_each_field_in_workload walks individual workload fields")
		{
			Model model;
			const WorkloadSeed& w = model.add("SimpleWorkload", "Solo").set_tick_rate_hz(10.0f);
			model.set_root_workload(w);

			Engine engine;
			engine.load(model);

			const auto& info = engine.get_all_instance_info()[0];

			int input_hits = 0;
			int output_hits = 0;
			WorkloadFieldsIterator::for_each_field_in_workload(engine,
				info,
				nullptr,
				[&](const WorkloadFieldView& view)
				{
					if (string_equals(view.field_info->name.c_str(), "input_value"))
						++input_hits;
					if (string_equals(view.field_info->name.c_str(), "output_value"))
						++output_hits;
				});

			CHECK(input_hits == 1);
			CHECK(output_hits == 1);
		}
	}

} // namespace robotick::test
