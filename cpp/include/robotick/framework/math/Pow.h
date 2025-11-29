// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/memory/StdApproved.h"

#include <cmath>

namespace robotick
{
	namespace internal::Pow
	{
		template <typename T> struct Fn;

		template <> struct Fn<float>
		{
			static inline float apply(float base, float exponent) { return ::powf(base, exponent); }
		};

		template <> struct Fn<double>
		{
			static inline double apply(double base, double exponent) { return ::pow(base, exponent); }
		};
	} // namespace internal::Pow

	template <typename Base, typename Exponent> inline auto pow(Base base, Exponent exponent)
	{
		using Common = typename std_approved::common_type<Base, Exponent>::type;
		return internal::Pow::Fn<Common>::apply(static_cast<Common>(base), static_cast<Common>(exponent));
	}
} // namespace robotick
