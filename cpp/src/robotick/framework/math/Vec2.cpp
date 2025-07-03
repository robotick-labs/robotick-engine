// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/math/Vec2.h"

#include "robotick/framework/registry/TypeMacros.h"

using namespace robotick;

// register Vec2f: =====

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec2f)
ROBOTICK_STRUCT_FIELD(Vec2f, float, x)
ROBOTICK_STRUCT_FIELD(Vec2f, float, y)
ROBOTICK_REGISTER_STRUCT_END(Vec2f)

// register Vec2d: =====

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec2d)
ROBOTICK_STRUCT_FIELD(Vec2d, double, x)
ROBOTICK_STRUCT_FIELD(Vec2d, double, y)
ROBOTICK_REGISTER_STRUCT_END(Vec2d)

// register Vec2: =====

#if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec2)
ROBOTICK_STRUCT_FIELD(Vec2, double, x)
ROBOTICK_STRUCT_FIELD(Vec2, double, y)
ROBOTICK_REGISTER_STRUCT_END(Vec2)

#else

ROBOTICK_REGISTER_STRUCT_BEGIN(Vec2)
ROBOTICK_STRUCT_FIELD(Vec2, float, x)
ROBOTICK_STRUCT_FIELD(Vec2, float, y)
ROBOTICK_REGISTER_STRUCT_END(Vec2)

#endif // #if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)