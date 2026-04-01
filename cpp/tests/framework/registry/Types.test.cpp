// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/config/AssertUtils.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringUtils.h"
#include "robotick/framework/strings/StringView.h"
#include <catch2/catch_all.hpp>
#include <cstring>

enum class RegistryTestSignedEnum : int32_t
{
	Negative = -5,
	Zero = 0,
	Positive = 12
};

ROBOTICK_REGISTER_ENUM_BEGIN(RegistryTestSignedEnum)
ROBOTICK_ENUM_VALUE("Negative", RegistryTestSignedEnum::Negative)
ROBOTICK_ENUM_VALUE("Zero", RegistryTestSignedEnum::Zero)
ROBOTICK_ENUM_VALUE("Positive", RegistryTestSignedEnum::Positive)
ROBOTICK_REGISTER_ENUM_END(RegistryTestSignedEnum)

enum RegistryTestUnsignedEnum : uint32_t
{
	RegistryTestUnsignedEnum_None = 0,
	RegistryTestUnsignedEnum_FlagA = 1u << 0,
	RegistryTestUnsignedEnum_FlagHigh = 0x80000000u
};

ROBOTICK_REGISTER_ENUM_BEGIN(RegistryTestUnsignedEnum)
ROBOTICK_ENUM_VALUE("None", RegistryTestUnsignedEnum_None)
ROBOTICK_ENUM_VALUE("FlagA", RegistryTestUnsignedEnum_FlagA)
ROBOTICK_ENUM_VALUE("FlagHigh", RegistryTestUnsignedEnum_FlagHigh)
ROBOTICK_REGISTER_ENUM_END(RegistryTestUnsignedEnum)

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
				const char* name_cstr = desc->name.c_str();
				for (size_t i = 0; i < name_count; ++i)
				{
					if (seen_names[i] == name_cstr)
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

		SECTION("Primitive Types - invalid bool strings and unsupported descriptors return false")
		{
			const TypeDescriptor* bool_desc = registry.find_by_name("bool");
			REQUIRE(bool_desc != nullptr);
			bool bool_value = false;
			CHECK_FALSE(bool_desc->from_string("not_a_boolean", &bool_value));

			TypeDescriptor custom_desc{StringView("custom"), TypeId::invalid(), sizeof(int), alignof(int), TypeCategory::Primitive, {}, nullptr};

			char buffer[32] = {};
			int payload = 0;
			CHECK_FALSE(custom_desc.to_string(&payload, buffer, sizeof(buffer)));
			CHECK_FALSE(custom_desc.from_string("123", &payload));
		}

		SECTION("Primitive Types - text/plain descriptors stay null terminated")
		{
			const TypeDescriptor* text_desc = registry.find_by_name("FixedString8");
			REQUIRE(text_desc != nullptr);

			FixedString8 dest;
			CHECK(text_desc->from_string("123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", dest.data));
			CHECK(dest.data[dest.capacity() - 1] == '\0');
		}

		SECTION("Primitive Types - text/plain from_string zero-fills remainder")
		{
			const TypeDescriptor* text_desc = registry.find_by_name("FixedString8");
			REQUIRE(text_desc != nullptr);

			const size_t dest_size = sizeof(FixedString8);
			char dest[dest_size];
			memset(dest, 'Z', dest_size);

			CHECK(text_desc->from_string("hello", dest));
			const size_t actual_len = robotick::string_length(dest);
			CHECK(actual_len < dest_size);
			for (size_t i = actual_len + 1; i < dest_size; ++i)
			{
				CHECK(dest[i] == '\0');
			}
		}

		SECTION("Primitive Types - to_string leaves buffer zeroed on overflow")
		{
			const TypeDescriptor* int_desc = registry.find_by_name("int");
			REQUIRE(int_desc != nullptr);

			char small_buffer[3];
			memset(small_buffer, 'X', sizeof(small_buffer));

			const int sample = 123456;
			CHECK_FALSE(int_desc->to_string(&sample, small_buffer, sizeof(small_buffer)));
			CHECK(small_buffer[0] == '\0');
		}

		SECTION("Enum Types - metadata and signed conversions")
		{
			const TypeDescriptor* signed_desc = registry.find_by_name("RegistryTestSignedEnum");
			REQUIRE(signed_desc != nullptr);
			CHECK(signed_desc->type_category == TypeCategory::Enum);

			const EnumDescriptor* enum_desc = signed_desc->get_enum_desc();
			REQUIRE(enum_desc != nullptr);
			CHECK(enum_desc->is_signed);
			CHECK(enum_desc->values.size() == 3);

			RegistryTestSignedEnum value = RegistryTestSignedEnum::Positive;
			char buffer[32] = {};
			REQUIRE(signed_desc->to_string(&value, buffer, sizeof(buffer)));
			CHECK(robotick::string_equals(buffer, "Positive"));

			RegistryTestSignedEnum parsed = RegistryTestSignedEnum::Zero;
			REQUIRE(signed_desc->from_string("Negative", &parsed));
			CHECK(parsed == RegistryTestSignedEnum::Negative);

			REQUIRE(signed_desc->from_string("37", &parsed));
			CHECK(static_cast<int>(parsed) == 37);

			parsed = static_cast<RegistryTestSignedEnum>(99);
			REQUIRE(signed_desc->to_string(&parsed, buffer, sizeof(buffer)));
			CHECK(robotick::string_equals(buffer, "99"));
		}

		SECTION("Enum Types - unsigned conversions and fallback")
		{
			const TypeDescriptor* unsigned_desc = registry.find_by_name("RegistryTestUnsignedEnum");
			REQUIRE(unsigned_desc != nullptr);
			CHECK(unsigned_desc->type_category == TypeCategory::Enum);

			const EnumDescriptor* enum_desc = unsigned_desc->get_enum_desc();
			REQUIRE(enum_desc != nullptr);
			CHECK_FALSE(enum_desc->is_signed);
			CHECK(enum_desc->values.size() == 3);

			RegistryTestUnsignedEnum value = RegistryTestUnsignedEnum_FlagHigh;
			char buffer[32] = {};
			REQUIRE(unsigned_desc->to_string(&value, buffer, sizeof(buffer)));
			CHECK(robotick::string_equals(buffer, "FlagHigh"));

			REQUIRE(unsigned_desc->from_string("FlagA", &value));
			CHECK(value == RegistryTestUnsignedEnum_FlagA);

			REQUIRE(unsigned_desc->from_string("2147483648", &value));
			REQUIRE(unsigned_desc->to_string(&value, buffer, sizeof(buffer)));
			CHECK(robotick::string_equals(buffer, "FlagHigh"));

			value = static_cast<RegistryTestUnsignedEnum>(0xDEADBEEFu);
			REQUIRE(unsigned_desc->to_string(&value, buffer, sizeof(buffer)));
			CHECK(robotick::string_equals(buffer, "3735928559"));
		}

		SECTION("TypeRegistry rejects duplicate ids")
		{
			if (TypeRegistry::get().is_sealed())
			{
				SKIP("TypeRegistry already sealed by earlier tests; run this section in an unsealed test context.");
			}

			static FixedString64 persistent_names[32];
			static size_t persistent_count = 0;
			const size_t primary_idx = persistent_count++;
			const size_t alias_idx = persistent_count++;
			ROBOTICK_ASSERT(primary_idx < 32 && alias_idx < 32);

			FixedString64& primary_name = persistent_names[primary_idx];
			FixedString64& alias_name = persistent_names[alias_idx];
			primary_name.format("RegistryDuplicate_%zu", primary_idx);
			alias_name.format("RegistryDuplicateAlias_%zu", primary_idx);

			const TypeDescriptor s_duplicate_primary{
				StringView(primary_name.c_str()), TypeId(primary_name.c_str()), sizeof(int), alignof(int), TypeCategory::Primitive, {}, nullptr};
			TypeRegistry::get().register_type(s_duplicate_primary);

			const TypeDescriptor s_duplicate_secondary{
				StringView(alias_name.c_str()), TypeId(primary_name.c_str()), sizeof(int), alignof(int), TypeCategory::Primitive, {}, nullptr};
			ROBOTICK_REQUIRE_ERROR_MSG(
				TypeRegistry::get().register_type(s_duplicate_secondary), "TypeRegistry::register_type() - cannot have multiple types with same id");
		}
	}
} // namespace robotick::test
