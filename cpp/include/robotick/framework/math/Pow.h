// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

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

	template <typename T> inline T pow(T base, T exponent)
	{
		return internal::Pow::Fn<T>::apply(base, exponent);
	}
} // namespace robotick
