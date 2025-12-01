// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>

namespace robotick
{
	namespace internal::Sqrt
	{
		template <typename T> struct Fn;

		template <> struct Fn<float>
		{
			static inline float apply(float value) { return ::sqrtf(value); }
		};

		template <> struct Fn<double>
		{
			static inline double apply(double value) { return ::sqrt(value); }
		};
	} // namespace internal::Sqrt

	template <typename T> inline T sqrt(T value)
	{
		return internal::Sqrt::Fn<T>::apply(value);
	}
} // namespace robotick
