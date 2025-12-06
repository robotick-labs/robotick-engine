// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/memory/StdApproved.h"

#include <algorithm>

namespace robotick
{
	template <typename It, typename Compare> inline void sort(It first, It last, Compare comp)
	{
		std_approved::sort(first, last, comp);
	}

	template <typename It> inline void sort(It first, It last)
	{
		std_approved::sort(first, last);
	}

	template <typename It, typename T> inline void fill(It first, It last, const T& value)
	{
		std_approved::fill(first, last, value);
	}
} // namespace robotick
