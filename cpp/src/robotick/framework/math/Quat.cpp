// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Quat.h"
#include "robotick/framework/registry/TypeMacros.h"

using namespace robotick;

// register Quatf: =====

ROBOTICK_REGISTER_STRUCT_BEGIN(Quatf)
ROBOTICK_STRUCT_FIELD(Quatf, float, w)
ROBOTICK_STRUCT_FIELD(Quatf, float, x)
ROBOTICK_STRUCT_FIELD(Quatf, float, y)
ROBOTICK_STRUCT_FIELD(Quatf, float, z)
ROBOTICK_REGISTER_STRUCT_END(Quatf)

// register Quatd: =====

ROBOTICK_REGISTER_STRUCT_BEGIN(Quatd)
ROBOTICK_STRUCT_FIELD(Quatd, double, w)
ROBOTICK_STRUCT_FIELD(Quatd, double, x)
ROBOTICK_STRUCT_FIELD(Quatd, double, y)
ROBOTICK_STRUCT_FIELD(Quatd, double, z)
ROBOTICK_REGISTER_STRUCT_END(Quatd)

extern "C" void robotick_force_register_quat_types()
{
	// This function exists solely to force this TU to be retained
}
