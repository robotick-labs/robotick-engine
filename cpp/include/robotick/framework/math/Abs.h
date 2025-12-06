// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/memory/StdApproved.h"

#include <cmath>
#include <cstdlib>

namespace robotick
{
	// We keep a small wrapper instead of aliasing STL abs so we can:
	//  * guarantee deterministic return types (signed values come back as their original type, unsigned pass through), and
	//  * emit a clear compile-time error when a type lacks a supported specialization instead of relying on ADL/linker errors.
	namespace internal::Abs
	{
		template <typename T, typename Enable = void> struct Fn
		{
			static_assert(sizeof(T) == 0, "robotick::abs not specialized for this type");
		};

		template <typename T> struct Fn<T, typename std_approved::enable_if<std_approved::is_integral<T>::value>::type>
		{
			static inline T apply(T value)
			{
				if constexpr (std_approved::is_unsigned<T>::value)
				{
					return value;
				}
				return static_cast<T>(std_approved::abs(value));
			}
		};

		template <> struct Fn<float, void>
		{
			static inline float apply(float value) { return ::fabsf(value); }
		};

		template <> struct Fn<double, void>
		{
			static inline double apply(double value) { return ::fabs(value); }
		};
	} // namespace internal::Abs

	template <typename T> inline T abs(T value)
	{
		return internal::Abs::Fn<T>::apply(value);
	}
} // namespace robotick
