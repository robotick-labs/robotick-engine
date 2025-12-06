// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>

namespace robotick
{
	namespace internal::LogExp
	{
		template <typename T> struct Log;
		template <typename T> struct Log2;
		template <typename T> struct Log10;
		template <typename T> struct Exp;

		template <> struct Log<float>
		{
			static inline float apply(float value) { return ::logf(value); }
		};

		template <> struct Log<double>
		{
			static inline double apply(double value) { return ::log(value); }
		};

		template <> struct Log2<float>
		{
			static inline float apply(float value) { return ::log2f(value); }
		};

		template <> struct Log2<double>
		{
			static inline double apply(double value) { return ::log2(value); }
		};

		template <> struct Log10<float>
		{
			static inline float apply(float value) { return ::log10f(value); }
		};

		template <> struct Log10<double>
		{
			static inline double apply(double value) { return ::log10(value); }
		};

		template <> struct Exp<float>
		{
			static inline float apply(float value) { return ::expf(value); }
		};

		template <> struct Exp<double>
		{
			static inline double apply(double value) { return ::exp(value); }
		};
	} // namespace internal::LogExp

	template <typename T> inline T log(T value)
	{
		return internal::LogExp::Log<T>::apply(value);
	}

	template <typename T> inline T log2(T value)
	{
		return internal::LogExp::Log2<T>::apply(value);
	}

	template <typename T> inline T log10(T value)
	{
		return internal::LogExp::Log10<T>::apply(value);
	}

	template <typename T> inline T exp(T value)
	{
		return internal::LogExp::Exp<T>::apply(value);
	}
} // namespace robotick
