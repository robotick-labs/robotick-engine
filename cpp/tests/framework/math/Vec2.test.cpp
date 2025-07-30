// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Vec2.h"

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

TEST_CASE("Unit/Framework/Math/Vec2")
{
	SECTION("Default construction and member access")
	{
		Vec2f v;
		REQUIRE(v.x == 0.0f);
		REQUIRE(v.y == 0.0f);
	}

	SECTION("Construction with values")
	{
		Vec2f v(1.0f, 2.0f);
		REQUIRE(v.x == 1.0f);
		REQUIRE(v.y == 2.0f);
	}

	SECTION("Addition and subtraction")
	{
		Vec2f a(1.0f, 2.0f);
		Vec2f b(0.5f, 1.5f);
		Vec2f c = a + b;
		Vec2f d = a - b;

		CHECK(c.x == Catch::Approx(1.5f));
		CHECK(c.y == Catch::Approx(3.5f));

		CHECK(d.x == Catch::Approx(0.5f));
		CHECK(d.y == Catch::Approx(0.5f));
	}

	SECTION("Scalar multiplication and division")
	{
		Vec2f v(1.0f, -2.0f);
		Vec2f mul = v * 2.0f;
		Vec2f div = v / 2.0f;

		CHECK(mul.x == Catch::Approx(2.0f));
		CHECK(mul.y == Catch::Approx(-4.0f));

		CHECK(div.x == Catch::Approx(0.5f));
		CHECK(div.y == Catch::Approx(-1.0f));
	}

	SECTION("Dot product")
	{
		Vec2f a(1.0f, 0.0f);
		Vec2f b(0.0f, 1.0f);
		Vec2f c(2.0f, 3.0f);

		CHECK(a.dot(b) == Catch::Approx(0.0f));
		CHECK(c.dot(c) == Catch::Approx(13.0f));
	}

	SECTION("Length")
	{
		Vec2f v(3.0f, 4.0f);
		CHECK(v.length_squared() == Catch::Approx(25.0f));
		CHECK(v.length() == Catch::Approx(5.0f));
	}

	SECTION("In-place operations")
	{
		Vec2f v(1.0f, 2.0f);
		v += Vec2f(1.0f, -1.0f);
		CHECK(v.x == Catch::Approx(2.0f));
		CHECK(v.y == Catch::Approx(1.0f));

		v *= 2.0f;
		CHECK(v.x == Catch::Approx(4.0f));
		CHECK(v.y == Catch::Approx(2.0f));
	}

	SECTION("Divide by zero triggers assert")
	{
		Vec2f v;
		ROBOTICK_REQUIRE_ERROR(v /= 0.0f);
		ROBOTICK_REQUIRE_ERROR(v / 0.0f);
	}

	SECTION("Types Are Registered")
	{
		const TypeDescriptor* type_descriptor_vec2f = TypeRegistry::get().find_by_id(GET_TYPE_ID(Vec2f));
		CHECK(type_descriptor_vec2f != nullptr);

		if (type_descriptor_vec2f)
		{
			CHECK(type_descriptor_vec2f->name == GET_TYPE_NAME(Vec2f));
			CHECK(type_descriptor_vec2f->id == GET_TYPE_ID(Vec2f));
			CHECK(type_descriptor_vec2f->type_category == TypeCategory::Struct);
			CHECK(type_descriptor_vec2f->type_category_desc.struct_desc != nullptr);
			if (type_descriptor_vec2f->type_category_desc.struct_desc)
			{
				CHECK(type_descriptor_vec2f->type_category_desc.struct_desc->fields.size() == 2);
			}
		}

		const TypeDescriptor* type_descriptor_vec2d = TypeRegistry::get().find_by_id(GET_TYPE_ID(Vec2d));
		CHECK(type_descriptor_vec2d != nullptr);

		if (type_descriptor_vec2d)
		{
			CHECK(type_descriptor_vec2d->name == GET_TYPE_NAME(Vec2d));
			CHECK(type_descriptor_vec2d->id == GET_TYPE_ID(Vec2d));
			CHECK(type_descriptor_vec2d->type_category == TypeCategory::Struct);
			CHECK(type_descriptor_vec2d->type_category_desc.struct_desc != nullptr);
			if (type_descriptor_vec2d->type_category_desc.struct_desc)
			{
				CHECK(type_descriptor_vec2d->type_category_desc.struct_desc->fields.size() == 2);
			}
		}

		const TypeDescriptor* type_descriptor_vec2 = TypeRegistry::get().find_by_id(GET_TYPE_ID(Vec2));
		CHECK(type_descriptor_vec2 != nullptr);

		if (type_descriptor_vec2)
		{
			CHECK(type_descriptor_vec2->name == GET_TYPE_NAME(Vec2));
			CHECK(type_descriptor_vec2->id == GET_TYPE_ID(Vec2));
			CHECK(type_descriptor_vec2->type_category == TypeCategory::Struct);
			CHECK(type_descriptor_vec2->type_category_desc.struct_desc != nullptr);
			if (type_descriptor_vec2->type_category_desc.struct_desc)
			{
				CHECK(type_descriptor_vec2->type_category_desc.struct_desc->fields.size() == 2);
			}
		}
	}
}
