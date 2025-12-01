// Copyright Robotick Labs
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

// register Quat (alias honors ROBOTICK_DEFAULT_REAL_IS_DOUBLE): =====

#if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)

ROBOTICK_REGISTER_STRUCT_BEGIN(Quat)
ROBOTICK_STRUCT_FIELD(Quat, double, w)
ROBOTICK_STRUCT_FIELD(Quat, double, x)
ROBOTICK_STRUCT_FIELD(Quat, double, y)
ROBOTICK_STRUCT_FIELD(Quat, double, z)
ROBOTICK_REGISTER_STRUCT_END(Quat)

#else

ROBOTICK_REGISTER_STRUCT_BEGIN(Quat)
ROBOTICK_STRUCT_FIELD(Quat, float, w)
ROBOTICK_STRUCT_FIELD(Quat, float, x)
ROBOTICK_STRUCT_FIELD(Quat, float, y)
ROBOTICK_STRUCT_FIELD(Quat, float, z)
ROBOTICK_REGISTER_STRUCT_END(Quat)

#endif // ROBOTICK_DEFAULT_REAL_IS_DOUBLE

extern "C" void robotick_force_register_quat_types()
{
	// This function exists solely to force this TU to be retained
}
