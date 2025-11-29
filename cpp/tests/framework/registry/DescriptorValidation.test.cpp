// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/config/AssertUtils.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/memory/HeapVector.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/strings/StringView.h"
#include "robotick/framework/utils/Constants.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		const TypeDescriptor s_zero_align_desc{
			StringView("ZeroAlignDescriptor"), TypeId("ZeroAlignDescriptor"), sizeof(int), 0, TypeCategory::Primitive, {}, nullptr};
		const AutoRegisterType s_zero_align_reg(s_zero_align_desc);

		const TypeDescriptor s_zero_size_desc{
			StringView("ZeroSizeDescriptor"), TypeId("ZeroSizeDescriptor"), 0, alignof(int), TypeCategory::Primitive, {}, nullptr};
		const AutoRegisterType s_zero_size_reg(s_zero_size_desc);
	} // namespace

	TEST_CASE("Unit/Framework/Registry/DescriptorValidation")
	{
		SECTION("Blackboard rejects invalid alignment descriptors")
		{
			FieldDescriptor invalid_field{StringView("alignment_bad"), s_zero_align_desc.id, OFFSET_UNBOUND, 1};
			HeapVector<FieldDescriptor> fields;
			fields.initialize(1);
			fields[0] = invalid_field;

			Blackboard blackboard;
			ROBOTICK_REQUIRE_ERROR_MSG(blackboard.initialize_fields(fields), "invalid alignment");
		}

		SECTION("Blackboard rejects zero-sized descriptors")
		{
			FieldDescriptor invalid_field{StringView("size_zero"), s_zero_size_desc.id, OFFSET_UNBOUND, 1};
			HeapVector<FieldDescriptor> fields;
			fields.initialize(1);
			fields[0] = invalid_field;

			Blackboard blackboard;
			ROBOTICK_REQUIRE_ERROR_MSG(blackboard.initialize_fields(fields), "zero-sized type");
		}
	}
} // namespace robotick::test
