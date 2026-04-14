// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Pose.h"

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

namespace
{
	constexpr float kHalfPi = 1.57079632679f;
}

TEST_CASE("Unit/Framework/Math/Posef")
{
	SECTION("Default construction and identity")
	{
		Posef pose;
		REQUIRE(pose.position == Vec3f(0.0f, 0.0f, 0.0f));
		REQUIRE(pose.orientation == Quatf::identity());
		REQUIRE(Posef::identity() == pose);
	}

	SECTION("Transform point")
	{
		const Posef pose(Vec3f(1.0f, 2.0f, 3.0f), Quatf::from_axis_angle(0.0f, 0.0f, 1.0f, kHalfPi));
		const Vec3f world_point = pose.transform_point(Vec3f(1.0f, 0.0f, 0.0f));

		CHECK(world_point.x == Catch::Approx(1.0f));
		CHECK(world_point.y == Catch::Approx(3.0f));
		CHECK(world_point.z == Catch::Approx(3.0f));
	}

	SECTION("Inverse undo composition")
	{
		const Posef pose(Vec3f(1.0f, -2.0f, 0.5f), Quatf::from_axis_angle(0.0f, 0.0f, 1.0f, 0.75f));
		const Posef identity = pose * pose.inverse();

		CHECK(identity.position.x == Catch::Approx(0.0f).margin(1e-4f));
		CHECK(identity.position.y == Catch::Approx(0.0f).margin(1e-4f));
		CHECK(identity.position.z == Catch::Approx(0.0f).margin(1e-4f));
		CHECK(identity.orientation.w == Catch::Approx(1.0f).margin(1e-4f));
		CHECK(identity.orientation.x == Catch::Approx(0.0f).margin(1e-4f));
		CHECK(identity.orientation.y == Catch::Approx(0.0f).margin(1e-4f));
		CHECK(identity.orientation.z == Catch::Approx(0.0f).margin(1e-4f));
	}

	SECTION("Composition")
	{
		const Posef a(Vec3f(1.0f, 0.0f, 0.0f), Quatf::from_axis_angle(0.0f, 0.0f, 1.0f, kHalfPi));
		const Posef b(Vec3f(1.0f, 0.0f, 0.0f), Quatf::identity());
		const Posef composed = a * b;

		CHECK(composed.position.x == Catch::Approx(1.0f).margin(1e-4f));
		CHECK(composed.position.y == Catch::Approx(1.0f).margin(1e-4f));
		CHECK(composed.position.z == Catch::Approx(0.0f).margin(1e-4f));
	}

	SECTION("Types Are Registered")
	{
		const TypeDescriptor* type_descriptor_posef = TypeRegistry::get().find_by_id(GET_TYPE_ID(Posef));
		CHECK(type_descriptor_posef != nullptr);
		if (type_descriptor_posef)
		{
			CHECK(type_descriptor_posef->name == GET_TYPE_NAME(Posef));
			CHECK(type_descriptor_posef->type_category == TypeCategory::Struct);
			REQUIRE(type_descriptor_posef->type_category_desc.struct_desc != nullptr);
			CHECK(type_descriptor_posef->type_category_desc.struct_desc->fields.size() == 2);
		}

		const TypeDescriptor* type_descriptor_posed = TypeRegistry::get().find_by_id(GET_TYPE_ID(Posed));
		CHECK(type_descriptor_posed != nullptr);
		if (type_descriptor_posed)
		{
			CHECK(type_descriptor_posed->name == GET_TYPE_NAME(Posed));
			CHECK(type_descriptor_posed->type_category == TypeCategory::Struct);
			REQUIRE(type_descriptor_posed->type_category_desc.struct_desc != nullptr);
			CHECK(type_descriptor_posed->type_category_desc.struct_desc->fields.size() == 2);
		}
	}
}
