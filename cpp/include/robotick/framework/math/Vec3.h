// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <math.h> // sqrtf, sqrt

namespace robotick
{
	// --- sqrt dispatch helper ---

	namespace internal::Vec3
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
	} // namespace internal::Vec3

	// --- Templated base class ---

	template <typename TDerived, typename TReal> struct Vec3Base
	{
		using TElement = TReal;

		TReal x = static_cast<TReal>(0);
		TReal y = static_cast<TReal>(0);
		TReal z = static_cast<TReal>(0);

		Vec3Base() = default;
		Vec3Base(TReal x, TReal y, TReal z)
			: x(x)
			, y(y)
			, z(z)
		{
		}

		TDerived operator+(const TDerived& rhs) const { return TDerived(x + rhs.x, y + rhs.y, z + rhs.z); }
		TDerived operator-(const TDerived& rhs) const { return TDerived(x - rhs.x, y - rhs.y, z - rhs.z); }
		TDerived operator*(TReal scalar) const { return TDerived(x * scalar, y * scalar, z * scalar); }
		TDerived operator/(TReal scalar) const
		{
			ROBOTICK_ASSERT_MSG((fabs(scalar) > static_cast<TReal>(kFloatEpsilon)), "Divide by zero requested!");
			return TDerived(x / scalar, y / scalar, z / scalar);
		}

		TDerived& operator+=(const TDerived& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator-=(const TDerived& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator*=(TReal scalar)
		{
			x *= scalar;
			y *= scalar;
			z *= scalar;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator/=(TReal scalar)
		{
			ROBOTICK_ASSERT_MSG((fabs(scalar) > static_cast<TReal>(kFloatEpsilon)), "Divide by zero requested (in-place)!");

			x /= scalar;
			y /= scalar;
			z /= scalar;
			return static_cast<TDerived&>(*this);
		}

		TReal dot(const TDerived& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }

		TDerived cross(const TDerived& rhs) const { return TDerived(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x); }

		TReal length_squared() const { return x * x + y * y + z * z; }

		TReal length() const { return internal::Vec3::SqrtFn<TReal>::apply(length_squared()); }
	};

	// --- Final types (can be forward-declared) ---

	struct Vec3f : public Vec3Base<Vec3f, float>
	{
		using Vec3Base::Vec3Base;
	};

	struct Vec3d : public Vec3Base<Vec3d, double>
	{
		using Vec3Base::Vec3Base;
	};

#if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)
	struct Vec3 : public Vec3Base<Vec3, double>
	{
		using Vec3Base::Vec3Base;
	};
#else
	struct Vec3 : public Vec3Base<Vec3, float>
	{
		using Vec3Base::Vec3Base;
	};
#endif

} // namespace robotick
