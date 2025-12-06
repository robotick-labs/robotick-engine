// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>

namespace robotick
{
	namespace internal::Trig
	{
		template <typename T> struct Sin;
		template <typename T> struct Cos;
		template <typename T> struct Atan2;

		template <> struct Sin<float>
		{
			static inline float apply(float value) { return ::sinf(value); }
		};

		template <> struct Sin<double>
		{
			static inline double apply(double value) { return ::sin(value); }
		};

		template <> struct Cos<float>
		{
			static inline float apply(float value) { return ::cosf(value); }
		};

		template <> struct Cos<double>
		{
			static inline double apply(double value) { return ::cos(value); }
		};

		template <> struct Atan2<float>
		{
			static inline float apply(float y, float x) { return ::atan2f(y, x); }
		};

		template <> struct Atan2<double>
		{
			static inline double apply(double y, double x) { return ::atan2(y, x); }
		};
	} // namespace internal::Trig

	inline float sin(float value)
	{
		return internal::Trig::Sin<float>::apply(value);
	}
	inline double sin(double value)
	{
		return internal::Trig::Sin<double>::apply(value);
	}

	inline float cos(float value)
	{
		return internal::Trig::Cos<float>::apply(value);
	}
	inline double cos(double value)
	{
		return internal::Trig::Cos<double>::apply(value);
	}

	inline float atan2(float y, float x)
	{
		return internal::Trig::Atan2<float>::apply(y, x);
	}
	inline double atan2(double y, double x)
	{
		return internal::Trig::Atan2<double>::apply(y, x);
	}
} // namespace robotick
