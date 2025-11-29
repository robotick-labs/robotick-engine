// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/NoStl.h"

#include <initializer_list>

namespace robotick
{
	template <typename T>
	using initializer_list = std_approved::initializer_list<T>;
} // namespace robotick
