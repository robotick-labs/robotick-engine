
// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/data/Blackboard_v2.h"
#include "robotick/framework/math/Vec3.h"
#include "robotick/framework/registry-v2/TypeMacros.h"
#include "robotick/framework/utils/TypeId.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		struct RegistryTestInputs
		{
			int input_value = 0;
			Vec3 input_vec3;
		};

		ROBOTICK_REGISTER_STRUCT_BEGIN(RegistryTestInputs)
		ROBOTICK_STRUCT_FIELD(RegistryTestInputs, int, input_value)
		ROBOTICK_STRUCT_FIELD(RegistryTestInputs, Vec3, input_vec3)
		ROBOTICK_REGISTER_STRUCT_END(RegistryTestInputs)

		struct RegistryTestOutputs
		{
			int output_value_1 = 0;
			int output_value_2 = 0;
			Vec3 output_vec3;
			Blackboard_v2 output_blackboard;
		};

		ROBOTICK_REGISTER_STRUCT_BEGIN(RegistryTestOutputs)
		ROBOTICK_STRUCT_FIELD(RegistryTestOutputs, int, output_value_1)
		ROBOTICK_STRUCT_FIELD(RegistryTestOutputs, int, output_value_2)
		ROBOTICK_STRUCT_FIELD(RegistryTestOutputs, Vec3, output_vec3)
		ROBOTICK_STRUCT_FIELD(RegistryTestOutputs, Blackboard_v2, output_blackboard)
		ROBOTICK_REGISTER_STRUCT_END(RegistryTestOutputs)

		struct RegistryTestState
		{
			HeapVector<FieldDescriptor> blackboard_fields;
		};

		struct RegistryTestWorkload
		{
			RegistryTestInputs inputs;
			RegistryTestOutputs outputs;

			State<RegistryTestState> state;

			void setup()
			{
				state->blackboard_fields.initialize(2);

				state->blackboard_fields[0] = FieldDescriptor{"counter", GET_TYPE_ID(int)};
				state->blackboard_fields[1] = FieldDescriptor{"target_position", GET_TYPE_ID(Vec3)};

				const ArrayView<FieldDescriptor> fields_view(state->blackboard_fields.data(), state->blackboard_fields.size());

				outputs.output_blackboard.initialize_fields(fields_view);
			}

			void tick(const TickInfo&) { outputs.output_value_1 = inputs.input_value + 1; }
		};

		ROBOTICK_REGISTER_WORKLOAD(RegistryTestWorkload, void, RegistryTestInputs, RegistryTestOutputs)
	} // namespace

	TEST_CASE("Unit/Framework/Registry/Workloads")
	{
		SECTION("Can find 'RegistryTestWorkload' as a workload in the registry")
		{
			const TypeDescriptor* workload_type = TypeRegistry::get().find_by_name("RegistryTestWorkload");
			REQUIRE(workload_type != nullptr);
			CHECK(workload_type->type_category == TypeDescriptor::TypeCategory::Workload);
		}

		SECTION("'RegistryTestWorkload' has correct metadata")
		{
			const TypeDescriptor* td = TypeRegistry::get().find_by_name("RegistryTestWorkload");
			REQUIRE(td != nullptr);
			CHECK(td->name == "RegistryTestWorkload");
			CHECK(td->id == GET_TYPE_ID(RegistryTestWorkload));
			CHECK(td->size == sizeof(RegistryTestWorkload));
			CHECK(td->alignment == alignof(RegistryTestWorkload));

			const WorkloadDescriptor* desc = td->type_category_desc.workload_desc;
			REQUIRE(desc != nullptr);
			CHECK(desc->start_fn == nullptr);
			CHECK(desc->tick_fn != nullptr);
			CHECK(desc->input_offset != SIZE_MAX);
			CHECK(desc->output_offset != SIZE_MAX);
		}

		SECTION("Can find 'RegistryTestInputs' struct in the registry with correct metadata")
		{
			const TypeDescriptor* td = TypeRegistry::get().find_by_name("RegistryTestInputs");
			REQUIRE(td != nullptr);
			CHECK(td->type_category == TypeDescriptor::TypeCategory::Struct);
			CHECK(td->id == GET_TYPE_ID(RegistryTestInputs));
			CHECK(td->size == sizeof(RegistryTestInputs));

			const StructDescriptor* sd = td->type_category_desc.struct_desc;
			REQUIRE(sd != nullptr);
			CHECK(sd->fields.size() == 2);
			CHECK(sd->fields[0].name == "input_value");
			CHECK(sd->fields[0].type_id == GET_TYPE_ID(int));
			CHECK(sd->fields[1].name == "input_vec3");
			CHECK(sd->fields[1].type_id == GET_TYPE_ID(Vec3));
		}

		SECTION("Can find 'RegistryTestOutputs' struct in the registry with correct metadata")
		{
			const TypeDescriptor* td = TypeRegistry::get().find_by_name("RegistryTestOutputs");
			REQUIRE(td != nullptr);
			CHECK(td->type_category == TypeDescriptor::TypeCategory::Struct);
			CHECK(td->id == GET_TYPE_ID(RegistryTestOutputs));
			CHECK(td->size == sizeof(RegistryTestOutputs));

			const StructDescriptor* sd = td->type_category_desc.struct_desc;
			REQUIRE(sd != nullptr);
			CHECK(sd->fields.size() == 4);
			CHECK(sd->fields[0].name == "output_value_1");
			CHECK(sd->fields[0].type_id == GET_TYPE_ID(int));
			CHECK(sd->fields[0].find_type_descriptor()->id == sd->fields[0].type_id);
			CHECK(sd->fields[1].name == "output_value_2");
			CHECK(sd->fields[1].type_id == GET_TYPE_ID(int));
			CHECK(sd->fields[1].find_type_descriptor()->id == sd->fields[1].type_id);
			CHECK(sd->fields[2].name == "output_vec3");
			CHECK(sd->fields[2].type_id == GET_TYPE_ID(Vec3));
			CHECK(sd->fields[2].find_type_descriptor()->id == sd->fields[2].type_id);
			CHECK(sd->fields[3].name == "output_blackboard");
			CHECK(sd->fields[3].type_id == GET_TYPE_ID(Blackboard_v2));
			CHECK(sd->fields[3].find_type_descriptor()->id == sd->fields[3].type_id);
		}

		SECTION("Blackboard fields correctly register and resolve")
		{
			RegistryTestWorkload test_workload;
			test_workload.setup();

			Blackboard_v2& blackboard = test_workload.outputs.output_blackboard;

			// Lookup the type descriptor from the registry
			const TypeDescriptor* blackboard_type = TypeRegistry::get().find_by_name("Blackboard_v2");
			REQUIRE(blackboard_type != nullptr);
			CHECK(blackboard_type->type_category == TypeDescriptor::TypeCategory::DynamicStruct);

			// Get the dynamic resolver and call it with our Blackboard
			const DynamicStructDescriptor* dyn_desc = blackboard_type->type_category_desc.dynamic_struct_desc;
			REQUIRE(dyn_desc != nullptr);

			const StructDescriptor* resolved_struct = dyn_desc->resolve_fn(&blackboard);
			REQUIRE(resolved_struct != nullptr);

			CHECK(resolved_struct->fields.size() == 2);

			CHECK(resolved_struct->fields[0].name == "counter");
			CHECK(resolved_struct->fields[0].type_id == GET_TYPE_ID(int));
			CHECK(resolved_struct->fields[0].find_type_descriptor()->id == GET_TYPE_ID(int));

			CHECK(resolved_struct->fields[1].name == "target_position");
			CHECK(resolved_struct->fields[1].type_id == GET_TYPE_ID(Vec3));
			CHECK(resolved_struct->fields[1].find_type_descriptor()->id == GET_TYPE_ID(Vec3));
		}

		SECTION("'RegistryTestWorkload' links correctly to input/output types from registry")
		{
			const TypeDescriptor* workload_type_desc = TypeRegistry::get().find_by_name("RegistryTestWorkload");
			REQUIRE(workload_type_desc != nullptr);
			const WorkloadDescriptor* workload_desc = workload_type_desc->type_category_desc.workload_desc;
			REQUIRE(workload_desc != nullptr);

			const TypeDescriptor* inputs_type = workload_desc->inputs_desc;
			const TypeDescriptor* expected_inputs = TypeRegistry::get().find_by_name("RegistryTestInputs");
			CHECK(inputs_type == expected_inputs); // Must point to same descriptor object

			const TypeDescriptor* outputs_type = workload_desc->outputs_desc;
			const TypeDescriptor* expected_outputs = TypeRegistry::get().find_by_name("RegistryTestOutputs");
			CHECK(outputs_type == expected_outputs); // Must point to same descriptor object
		}
	}

} // namespace robotick::test
