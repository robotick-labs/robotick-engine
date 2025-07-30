// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Vec3.h"

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

TEST_CASE("Unit/Framework/Math/Vec3")
{
	SECTION("Default construction and member access")
	{
		Vec3f v;
		REQUIRE(v.x == 0.0f);
		REQUIRE(v.y == 0.0f);
		REQUIRE(v.z == 0.0f);
	}

	SECTION("Construction with values")
	{
		Vec3f v(1.0f, 2.0f, 3.0f);
		REQUIRE(v.x == 1.0f);
		REQUIRE(v.y == 2.0f);
		REQUIRE(v.z == 3.0f);
	}

	SECTION("Addition and subtraction")
	{
		Vec3f a(1.0f, 2.0f, 3.0f);
		Vec3f b(0.5f, 1.5f, -1.0f);
		Vec3f c = a + b;
		Vec3f d = a - b;

		CHECK(c.x == Catch::Approx(1.5f));
		CHECK(c.y == Catch::Approx(3.5f));
		CHECK(c.z == Catch::Approx(2.0f));

		CHECK(d.x == Catch::Approx(0.5f));
		CHECK(d.y == Catch::Approx(0.5f));
		CHECK(d.z == Catch::Approx(4.0f));
	}

	SECTION("Scalar multiplication and division")
	{
		Vec3f v(1.0f, -2.0f, 4.0f);
		Vec3f mul = v * 2.0f;
		Vec3f div = v / 2.0f;

		CHECK(mul.x == Catch::Approx(2.0f));
		CHECK(mul.y == Catch::Approx(-4.0f));
		CHECK(mul.z == Catch::Approx(8.0f));

		CHECK(div.x == Catch::Approx(0.5f));
		CHECK(div.y == Catch::Approx(-1.0f));
		CHECK(div.z == Catch::Approx(2.0f));
	}

	SECTION("Dot product")
	{
		Vec3f a(1.0f, 0.0f, 0.0f);
		Vec3f b(0.0f, 1.0f, 0.0f);
		Vec3f c(2.0f, 3.0f, 4.0f);

		CHECK(a.dot(b) == Catch::Approx(0.0f));
		CHECK(c.dot(c) == Catch::Approx(29.0f));
	}

	SECTION("Cross product")
	{
		Vec3f x(1.0f, 0.0f, 0.0f);
		Vec3f y(0.0f, 1.0f, 0.0f);
		Vec3f z = x.cross(y);

		CHECK(z.x == Catch::Approx(0.0f));
		CHECK(z.y == Catch::Approx(0.0f));
		CHECK(z.z == Catch::Approx(1.0f));
	}

	SECTION("Length")
	{
		Vec3f v(3.0f, 4.0f, 0.0f);
		CHECK(v.length_squared() == Catch::Approx(25.0f));
		CHECK(v.length() == Catch::Approx(5.0f));
	}

	SECTION("In-place operations")
	{
		Vec3f v(1.0f, 2.0f, 3.0f);
		v += Vec3f(1.0f, -1.0f, 0.0f);
		CHECK(v.x == Catch::Approx(2.0f));
		CHECK(v.y == Catch::Approx(1.0f));
		CHECK(v.z == Catch::Approx(3.0f));

		v *= 2.0f;
		CHECK(v.x == Catch::Approx(4.0f));
		CHECK(v.y == Catch::Approx(2.0f));
		CHECK(v.z == Catch::Approx(6.0f));
	}

	SECTION("Divide by zero triggers assert")
	{
		Vec3f v;
		ROBOTICK_REQUIRE_ERROR(v /= 0.0f);

		ROBOTICK_REQUIRE_ERROR(v / 0.0f);
	}

	SECTION("Types Are Registered")
	{
		const TypeDescriptor* type_descriptor_vec3f = TypeRegistry::get().find_by_id(GET_TYPE_ID(Vec3f));
		CHECK(type_descriptor_vec3f != nullptr);

		if (type_descriptor_vec3f)
		{
			CHECK(type_descriptor_vec3f->name == GET_TYPE_NAME(Vec3f));
			CHECK(type_descriptor_vec3f->id == GET_TYPE_ID(Vec3f));
			CHECK(type_descriptor_vec3f->type_category == TypeCategory::Struct);
			CHECK(type_descriptor_vec3f->type_category_desc.struct_desc != nullptr);
			if (type_descriptor_vec3f->type_category_desc.struct_desc)
			{
				CHECK(type_descriptor_vec3f->type_category_desc.struct_desc->fields.size() == 3);
			}
		}

		const TypeDescriptor* type_descriptor_vec3d = TypeRegistry::get().find_by_id(GET_TYPE_ID(Vec3d));
		CHECK(type_descriptor_vec3d != nullptr);

		if (type_descriptor_vec3d)
		{
			CHECK(type_descriptor_vec3d->name == GET_TYPE_NAME(Vec3d));
			CHECK(type_descriptor_vec3d->id == GET_TYPE_ID(Vec3d));
			CHECK(type_descriptor_vec3d->type_category == TypeCategory::Struct);
			CHECK(type_descriptor_vec3d->type_category_desc.struct_desc != nullptr);
			if (type_descriptor_vec3d->type_category_desc.struct_desc)
			{
				CHECK(type_descriptor_vec3d->type_category_desc.struct_desc->fields.size() == 3);
			}
		}

		const TypeDescriptor* type_descriptor_vec3 = TypeRegistry::get().find_by_id(GET_TYPE_ID(Vec3));
		CHECK(type_descriptor_vec3 != nullptr);

		if (type_descriptor_vec3)
		{
			CHECK(type_descriptor_vec3->name == GET_TYPE_NAME(Vec3));
			CHECK(type_descriptor_vec3->id == GET_TYPE_ID(Vec3));
			CHECK(type_descriptor_vec3->type_category == TypeCategory::Struct);
			CHECK(type_descriptor_vec3->type_category_desc.struct_desc != nullptr);
			if (type_descriptor_vec3->type_category_desc.struct_desc)
			{
				CHECK(type_descriptor_vec3->type_category_desc.struct_desc->fields.size() == 3);
			}
		}
	}
}
