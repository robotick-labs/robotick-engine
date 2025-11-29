// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>

namespace robotick
{
	namespace internal::Abs
	{
		template <typename T> struct Fn;

		template <> struct Fn<float>
		{
			static inline float apply(float value) { return ::fabsf(value); }
		};

		template <> struct Fn<double>
		{
			static inline double apply(double value) { return ::fabs(value); }
		};
	} // namespace internal::Abs

	template <typename T> inline T abs(T value)
	{
		return internal::Abs::Fn<T>::apply(value);
	}
} // namespace robotick
