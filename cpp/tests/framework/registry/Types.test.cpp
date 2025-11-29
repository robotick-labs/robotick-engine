// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/common/StringUtils.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	struct ExpectedType
	{
		const char* name;
		TypeId id;
		size_t size;
		const void* sample_value;
	};

	static const int i_val = 42;
	static const float f_val = 3.14f;
	static const double d_val = 2.718;
	static const bool b_val = true;
	static const FixedString8 fs8_val = "hi";
	static const FixedString16 fs16_val = "ok";
	static const FixedString32 fs32_val = "hello";
	static const FixedString64 fs64_val = "longer";
	static const FixedString128 fs128_val = "alpha";
	static const FixedString256 fs256_val = "bravo";
	static const FixedString512 fs512_val = "charlie";
	static const FixedString1024 fs1024_val = "delta";

	static const ExpectedType expected_primitive_types[] = {
		{"int", GET_TYPE_ID(int), sizeof(int), &i_val},
		{"float", GET_TYPE_ID(float), sizeof(float), &f_val},
		{"double", GET_TYPE_ID(double), sizeof(double), &d_val},
		{"bool", GET_TYPE_ID(bool), sizeof(bool), &b_val},
		{"FixedString8", GET_TYPE_ID(FixedString8), sizeof(FixedString8), &fs8_val},
		{"FixedString16", GET_TYPE_ID(FixedString16), sizeof(FixedString16), &fs16_val},
		{"FixedString32", GET_TYPE_ID(FixedString32), sizeof(FixedString32), &fs32_val},
		{"FixedString64", GET_TYPE_ID(FixedString64), sizeof(FixedString64), &fs64_val},
		{"FixedString128", GET_TYPE_ID(FixedString128), sizeof(FixedString128), &fs128_val},
		{"FixedString256", GET_TYPE_ID(FixedString256), sizeof(FixedString256), &fs256_val},
		{"FixedString512", GET_TYPE_ID(FixedString512), sizeof(FixedString512), &fs512_val},
		{"FixedString1024", GET_TYPE_ID(FixedString1024), sizeof(FixedString1024), &fs1024_val},
	};

	TEST_CASE("Unit/Framework/Registry/Types")
	{
		auto& registry = TypeRegistry::get();

		SECTION("All Types - names and ids are unique")
		{
			const auto& registered_types = registry.get_registered_types();

			HeapVector<FixedString64> seen_names;
			HeapVector<TypeId> seen_ids;
			seen_names.initialize(registered_types.size());
			seen_ids.initialize(registered_types.size());
			size_t name_count = 0;
			size_t id_count = 0;

			for (auto desc : registered_types)
			{
				REQUIRE(desc != nullptr);
				bool name_unique = true;
				for (size_t i = 0; i < name_count; ++i)
				{
					if (string_equals(seen_names[i].c_str(), desc->name.c_str()))
					{
						name_unique = false;
						break;
					}
				}
				REQUIRE(name_unique);
				seen_names[name_count++] = desc->name.c_str();

				bool id_unique = true;
				for (size_t j = 0; j < id_count; ++j)
				{
					if (seen_ids[j] == desc->id)
					{
						id_unique = false;
						break;
					}
				}
				REQUIRE(id_unique);
				seen_ids[id_count++] = desc->id;
			}
		}

		SECTION("Primitive Types - expected sizes and ids are present and correct")
		{
			for (const auto& entry : expected_primitive_types)
			{
				const TypeDescriptor* desc = registry.find_by_name(entry.name);
				REQUIRE(desc != nullptr);
				CHECK(desc->id == entry.id);
				CHECK(desc->size == entry.size);
				CHECK(desc->type_category == TypeCategory::Primitive);
			}
		}

		SECTION("Primitive Types - roundtrip to_string and from_string works for each")
		{
			static const size_t temp_buffers_size = 2048;

			char buffer[temp_buffers_size]; // more thanks large enough for any expected_primitive_types type

			for (const auto& entry : expected_primitive_types)
			{
				const TypeDescriptor* desc = registry.find_by_name(entry.name);
				REQUIRE(desc != nullptr);

				// to_string
				CHECK(desc->to_string(entry.sample_value, buffer, sizeof(buffer)));

				// from_string
				uint8_t temp_storage[temp_buffers_size] = {}; // more thanks large enough for any expected_primitive_types type

				void* parsed = temp_storage;
				CHECK(desc->from_string(buffer, parsed));

				// compare raw bytes
				CHECK(memcmp(parsed, entry.sample_value, desc->size) == 0);
			}
		}
	}
} // namespace robotick::test
