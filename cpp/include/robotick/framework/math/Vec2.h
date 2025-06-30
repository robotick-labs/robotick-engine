// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <math.h> // sqrtf, sqrt

namespace robotick
{
	// --- sqrt dispatch helper ---

	namespace internal
	{
		template <typename T> struct SqrtFn;

		template <> struct SqrtFn<float>
		{
			static inline float apply(float v) { return sqrtf(v); }
		};
		template <> struct SqrtFn<double>
		{
			static inline double apply(double v) { return sqrt(v); }
		};
	} // namespace internal

	// --- Templated base class ---

	template <typename TDerived, typename TReal> struct Vec2Base
	{
		using TElement = TReal;

		TReal x = static_cast<TReal>(0);
		TReal y = static_cast<TReal>(0);

		Vec2Base() = default;
		Vec2Base(TReal x, TReal y)
			: x(x)
			, y(y)
		{
		}

		TDerived operator+(const TDerived& rhs) const { return TDerived(x + rhs.x, y + rhs.y); }
		TDerived operator-(const TDerived& rhs) const { return TDerived(x - rhs.x, y - rhs.y); }
		TDerived operator*(TReal scalar) const { return TDerived(x * scalar, y * scalar); }
		TDerived operator/(TReal scalar) const
		{
			ROBOTICK_ASSERT_MSG((scalar != TReal(0)), "Divide by zero requested!");
			return TDerived(x / scalar, y / scalar);
		}

		TDerived& operator+=(const TDerived& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator-=(const TDerived& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator*=(TReal scalar)
		{
			x *= scalar;
			y *= scalar;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator/=(TReal scalar)
		{
			ROBOTICK_ASSERT_MSG((scalar != TReal(0)), "Divide by zero requested (in-place)!");
			x /= scalar;
			y /= scalar;
			return static_cast<TDerived&>(*this);
		}

		TReal dot(const TDerived& rhs) const { return x * rhs.x + y * rhs.y; }

		TReal length_squared() const { return x * x + y * y; }

		TReal length() const { return internal::SqrtFn<TReal>::apply(length_squared()); }

		TDerived normalized() const
		{
			TReal len = length();
			return (len > TReal(0)) ? (*static_cast<const TDerived*>(this) / len) : TDerived();
		}

		void normalize()
		{
			TReal len = length();
			if (len > TReal(0))
			{
				x /= len;
				y /= len;
			}
		}
	};

	// --- Final types (can be forward-declared) ---

	struct Vec2f : public Vec2Base<Vec2f, float>
	{
		using Vec2Base::Vec2Base;
	};

	struct Vec2d : public Vec2Base<Vec2d, double>
	{
		using Vec2Base::Vec2Base;
	};

#if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)
	struct Vec2 : public Vec2Base<Vec2, double>
	{
		using Vec2Base::Vec2Base;
	};
#else
	struct Vec2 : public Vec2Base<Vec2, float>
	{
		using Vec2Base::Vec2Base;
	};
#endif

} // namespace robotick
