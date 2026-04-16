// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Transform.h"

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

namespace
{
	constexpr double kHalfPi = 1.5707963267948966;
}

TEST_CASE("Unit/Framework/Math/Transformd")
{
	SECTION("Default construction and identity")
	{
		Transformd transform;
		REQUIRE(transform.translation == Vec3d(0.0, 0.0, 0.0));
		REQUIRE(transform.rotation == Quatd::identity());
		REQUIRE(Transformd::identity() == transform);
	}

	SECTION("Transform point")
	{
		const Transformd transform(Vec3d(2.0, 0.0, 1.0), Quatd::from_axis_angle(0.0, 0.0, 1.0, kHalfPi));
		const Vec3d world_point = transform.transform_point(Vec3d(1.0, 0.0, 0.0));

		CHECK(world_point.x == Catch::Approx(2.0));
		CHECK(world_point.y == Catch::Approx(1.0));
		CHECK(world_point.z == Catch::Approx(1.0));
	}

	SECTION("Inverse undo composition")
	{
		const Transformd transform(Vec3d(-1.0, 0.5, 3.0), Quatd::from_axis_angle(0.0, 0.0, 1.0, 0.5));
		const Transformd identity = transform * transform.inverse();

		CHECK(identity.translation.x == Catch::Approx(0.0).margin(1e-9));
		CHECK(identity.translation.y == Catch::Approx(0.0).margin(1e-9));
		CHECK(identity.translation.z == Catch::Approx(0.0).margin(1e-9));
		CHECK(identity.rotation.w == Catch::Approx(1.0).margin(1e-9));
		CHECK(identity.rotation.x == Catch::Approx(0.0).margin(1e-9));
		CHECK(identity.rotation.y == Catch::Approx(0.0).margin(1e-9));
		CHECK(identity.rotation.z == Catch::Approx(0.0).margin(1e-9));
	}

	SECTION("Types Are Registered")
	{
		const TypeDescriptor* type_descriptor_transformf = TypeRegistry::get().find_by_id(GET_TYPE_ID(Transformf));
		CHECK(type_descriptor_transformf != nullptr);
		if (type_descriptor_transformf)
		{
			CHECK(type_descriptor_transformf->name == GET_TYPE_NAME(Transformf));
			CHECK(type_descriptor_transformf->type_category == TypeCategory::Struct);
			REQUIRE(type_descriptor_transformf->type_category_desc.struct_desc != nullptr);
			CHECK(type_descriptor_transformf->type_category_desc.struct_desc->fields.size() == 2);
		}

		const TypeDescriptor* type_descriptor_transformd = TypeRegistry::get().find_by_id(GET_TYPE_ID(Transformd));
		CHECK(type_descriptor_transformd != nullptr);
		if (type_descriptor_transformd)
		{
			CHECK(type_descriptor_transformd->name == GET_TYPE_NAME(Transformd));
			CHECK(type_descriptor_transformd->type_category == TypeCategory::Struct);
			REQUIRE(type_descriptor_transformd->type_category_desc.struct_desc != nullptr);
			CHECK(type_descriptor_transformd->type_category_desc.struct_desc->fields.size() == 2);
		}
	}
}
