// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Pose.h"

#include "robotick/framework/registry/TypeMacros.h"

using namespace robotick;

ROBOTICK_REGISTER_STRUCT_BEGIN(Posef)
ROBOTICK_STRUCT_FIELD(Posef, Vec3f, position)
ROBOTICK_STRUCT_FIELD(Posef, Quatf, orientation)
ROBOTICK_REGISTER_STRUCT_END(Posef)

ROBOTICK_REGISTER_STRUCT_BEGIN(Posed)
ROBOTICK_STRUCT_FIELD(Posed, Vec3d, position)
ROBOTICK_STRUCT_FIELD(Posed, Quatd, orientation)
ROBOTICK_REGISTER_STRUCT_END(Posed)

extern "C" void robotick_force_register_pose_types()
{
	// This function exists solely to force this TU to be retained
}
