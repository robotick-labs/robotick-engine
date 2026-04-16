// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Transform.h"

#include "robotick/framework/registry/TypeMacros.h"

using namespace robotick;

ROBOTICK_REGISTER_STRUCT_BEGIN(Transformf)
ROBOTICK_STRUCT_FIELD(Transformf, Vec3f, translation)
ROBOTICK_STRUCT_FIELD(Transformf, Quatf, rotation)
ROBOTICK_REGISTER_STRUCT_END(Transformf)

ROBOTICK_REGISTER_STRUCT_BEGIN(Transformd)
ROBOTICK_STRUCT_FIELD(Transformd, Vec3d, translation)
ROBOTICK_STRUCT_FIELD(Transformd, Quatd, rotation)
ROBOTICK_REGISTER_STRUCT_END(Transformd)

extern "C" void robotick_force_register_transform_types()
{
	// This function exists solely to force this TU to be retained
}
