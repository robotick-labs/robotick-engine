// Copyright Robotick contributors
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

extern "C" void robotick_force_register_vec2_types()
{
	// This function exists solely to force this TU to be retained
}
