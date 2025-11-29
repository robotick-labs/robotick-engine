// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <functional>

namespace robotick
{
	template <typename Signature> using Function = std::function<Signature>;
} // namespace robotick
