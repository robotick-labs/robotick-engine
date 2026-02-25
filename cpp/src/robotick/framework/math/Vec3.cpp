// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Vec3.h"

#include "robotick/framework/registry/TypeMacros.h"

using namespace robotick;

// register Vec3f: =====

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec3f)
ROBOTICK_STRUCT_FIELD(Vec3f, float, x)
ROBOTICK_STRUCT_FIELD(Vec3f, float, y)
ROBOTICK_STRUCT_FIELD(Vec3f, float, z)
ROBOTICK_REGISTER_STRUCT_END(Vec3f)

// register Vec3d: =====

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec3d)
ROBOTICK_STRUCT_FIELD(Vec3d, double, x)
ROBOTICK_STRUCT_FIELD(Vec3d, double, y)
ROBOTICK_STRUCT_FIELD(Vec3d, double, z)
ROBOTICK_REGISTER_STRUCT_END(Vec3d)

extern "C" void robotick_force_register_vec3_types()
{
	// This function exists solely to force this TU to be retained
}
