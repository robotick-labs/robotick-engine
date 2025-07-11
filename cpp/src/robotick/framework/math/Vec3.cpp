// Copyright Robotick Labs
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

// register Vec3: =====

#if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec3)
ROBOTICK_STRUCT_FIELD(Vec3, double, x)
ROBOTICK_STRUCT_FIELD(Vec3, double, y)
ROBOTICK_STRUCT_FIELD(Vec3, double, z)
ROBOTICK_REGISTER_STRUCT_END(Vec3)

#else

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec3)
ROBOTICK_STRUCT_FIELD(Vec3, float, x)
ROBOTICK_STRUCT_FIELD(Vec3, float, y)
ROBOTICK_STRUCT_FIELD(Vec3, float, z)
ROBOTICK_REGISTER_STRUCT_END(Vec3)

#endif // #if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)