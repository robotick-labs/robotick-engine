// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/StdApproved.h"
#include <functional>

namespace robotick
{
	template <typename Signature> using Function = std_approved::function<Signature>;
} // namespace robotick
