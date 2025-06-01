
// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		struct RegistryTestInputs
		{
			int input_value = 0;
		};

		ROBOTICK_BEGIN_FIELDS(RegistryTestInputs)
		ROBOTICK_FIELD(RegistryTestInputs, input_value)
		ROBOTICK_END_FIELDS()

		struct RegistryTestOutputs
		{
			int output_value_1 = 0;
			int output_value_2 = 0;
		};

		ROBOTICK_BEGIN_FIELDS(RegistryTestOutputs)
		ROBOTICK_FIELD(RegistryTestOutputs, output_value_1)
		ROBOTICK_FIELD(RegistryTestOutputs, output_value_2)
		ROBOTICK_END_FIELDS()

		struct RegistryTestWorkload
		{
			RegistryTestInputs inputs;
			RegistryTestOutputs outputs;

			void tick(double) { outputs.output_value_1 = inputs.input_value + 1; }
		};

		ROBOTICK_DEFINE_WORKLOAD(RegistryTestWorkload, void, RegistryTestInputs, RegistryTestOutputs)
	} // namespace

	TEST_CASE("Unit|Registry|Workload and struct/field registration works")
	{
		const WorkloadRegistryEntry* workload = WorkloadRegistry::get().find("RegistryTestWorkload");
		REQUIRE(workload != nullptr);

		// Check workload metadata
		CHECK(workload->name == "RegistryTestWorkload");
		CHECK(workload->type_id == TypeId(GET_TYPE_ID(RegistryTestWorkload)));
		CHECK(workload->size == sizeof(RegistryTestWorkload));
		CHECK(workload->alignment == alignof(RegistryTestWorkload));
		CHECK(workload->tick_fn != nullptr);

		// Check input struct
		REQUIRE(workload->input_struct != nullptr);
		const auto* inputs = workload->input_struct;
		CHECK(inputs->name == "RegistryTestInputs");
		CHECK(inputs->type == TypeId(GET_TYPE_ID(RegistryTestInputs)));
		CHECK(inputs->size == sizeof(RegistryTestInputs));
		CHECK(inputs->fields.size() == 1);
		CHECK(inputs->field_from_name.count("input_value") == 1);

		// Check output struct
		REQUIRE(workload->output_struct != nullptr);
		const auto* outputs = workload->output_struct;
		CHECK(outputs->name == "RegistryTestOutputs");
		CHECK(outputs->type == TypeId(GET_TYPE_ID(RegistryTestOutputs)));
		CHECK(outputs->size == sizeof(RegistryTestOutputs));
		CHECK(outputs->fields.size() == 2);
		CHECK(outputs->field_from_name.count("output_value_1") == 1);
		CHECK(outputs->field_from_name.count("output_value_2") == 1);
	}

} // namespace robotick::test
