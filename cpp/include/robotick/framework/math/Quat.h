// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <float.h> // FLT_EPSILON, DBL_EPSILON
#include <math.h>  // sqrtf, sqrt

namespace robotick
{
	// --- sqrt dispatch helper ---

	namespace internal::Quat
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

		template <typename T> struct Epsilon;

		template <> struct Epsilon<float>
		{
			static constexpr float value = FLT_EPSILON;
		};
		template <> struct Epsilon<double>
		{
			static constexpr double value = DBL_EPSILON;
		};
	} // namespace internal::Quat

	// --- Templated base class ---

	template <typename TDerived, typename TReal> struct QuatBase
	{
		using TElement = TReal;

		// Stored as (w, x, y, z)
		TReal w = static_cast<TReal>(1);
		TReal x = static_cast<TReal>(0);
		TReal y = static_cast<TReal>(0);
		TReal z = static_cast<TReal>(0);

		QuatBase() = default;
		QuatBase(TReal w, TReal x, TReal y, TReal z)
			: w(w)
			, x(x)
			, y(y)
			, z(z)
		{
		}

		// Identity
		static TDerived identity() { return TDerived(static_cast<TReal>(1), static_cast<TReal>(0), static_cast<TReal>(0), static_cast<TReal>(0)); }

		// Comparisons
		bool operator==(const TDerived& rhs) const { return w == rhs.w && x == rhs.x && y == rhs.y && z == rhs.z; }
		bool operator!=(const TDerived& rhs) const { return !(*this == rhs); }

		// Basic arithmetic
		TDerived operator+(const TDerived& rhs) const { return TDerived(w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z); }
		TDerived operator-(const TDerived& rhs) const { return TDerived(w - rhs.w, x - rhs.x, y - rhs.y, z - rhs.z); }
		TDerived operator*(TReal s) const { return TDerived(w * s, x * s, y * s, z * s); }
		TDerived operator/(TReal s) const
		{
			ROBOTICK_ASSERT_MSG(fabs(s) > static_cast<TReal>(kFloatEpsilon), "Divide by zero requested!");
			return TDerived(w / s, x / s, y / s, z / s);
		}

		TDerived& operator+=(const TDerived& rhs)
		{
			w += rhs.w;
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator-=(const TDerived& rhs)
		{
			w -= rhs.w;
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator*=(TReal s)
		{
			w *= s;
			x *= s;
			y *= s;
			z *= s;
			return static_cast<TDerived&>(*this);
		}
		TDerived& operator/=(TReal s)
		{
			ROBOTICK_ASSERT_MSG(fabs(s) > static_cast<TReal>(kFloatEpsilon), "Divide by zero requested (in-place)!");
			w /= s;
			x /= s;
			y /= s;
			z /= s;
			return static_cast<TDerived&>(*this);
		}

		// Hamilton product (composition): this ∘ rhs
		TDerived operator*(const TDerived& rhs) const
		{
			// (w1,x1,y1,z1)*(w2,x2,y2,z2) = (w,x,y,z)
			const TReal nw = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
			const TReal nx = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
			const TReal ny = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
			const TReal nz = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
			return TDerived(nw, nx, ny, nz);
		}
		TDerived& operator*=(const TDerived& rhs) { return (*this = (*this * rhs)); }

		// Dot, norm, normalize
		TReal dot(const TDerived& rhs) const { return w * rhs.w + x * rhs.x + y * rhs.y + z * rhs.z; }

		TReal length_squared() const { return dot(static_cast<const TDerived&>(*this)); }

		TReal length() const { return internal::Quat::SqrtFn<TReal>::apply(length_squared()); }

		void normalize()
		{
			const TReal n = length();
			const TReal eps = static_cast<TReal>(internal::Quat::Epsilon<TReal>::value);
			if (n > eps)
			{
				const TReal inv = static_cast<TReal>(1) / n;
				w *= inv;
				x *= inv;
				y *= inv;
				z *= inv;
			}
			else
			{
				// Degenerate: fallback to identity
				w = static_cast<TReal>(1);
				x = y = z = static_cast<TReal>(0);
			}
		}

		TDerived normalized() const
		{
			TDerived q(this->w, this->x, this->y, this->z);
			return q / q.length();
		}

		// Conjugate / inverse
		TDerived conjugate() const { return TDerived(w, -x, -y, -z); }

		TDerived inverse() const
		{
			const TReal n2 = length_squared();
			const TReal eps = static_cast<TReal>(internal::Quat::Epsilon<TReal>::value);
			if (n2 > eps)
			{
				const TReal inv = static_cast<TReal>(1) / n2;
				return TDerived(w * inv, -x * inv, -y * inv, -z * inv);
			}
			// Degenerate: return identity
			return identity();
		}

		// Factory: from axis-angle (axis assumed normalized)
		static TDerived from_axis_angle(TReal axis_x, TReal axis_y, TReal axis_z, TReal angle_rad)
		{
			const TReal half = static_cast<TReal>(0.5) * angle_rad;
			const TReal s = sin(half);
			const TReal c = cos(half);
			return TDerived(c, axis_x * s, axis_y * s, axis_z * s);
		}

		// Factory: from ZYX Euler (yaw, pitch, roll)
		static TDerived from_euler_zyx(TReal yaw, TReal pitch, TReal roll)
		{
			const TReal cy = cos(yaw * 0.5);
			const TReal sy = sin(yaw * 0.5);
			const TReal cp = cos(pitch * 0.5);
			const TReal sp = sin(pitch * 0.5);
			const TReal cr = cos(roll * 0.5);
			const TReal sr = sin(roll * 0.5);

			// q = Rz(yaw) * Ry(pitch) * Rx(roll)
			TReal qw = cr * cp * cy + sr * sp * sy;
			TReal qx = sr * cp * cy - cr * sp * sy;
			TReal qy = cr * sp * cy + sr * cp * sy;
			TReal qz = cr * cp * sy - sr * sp * cy;

			return TDerived(qw, qx, qy, qz);
		}
	};

	// --- Final types (can be forward-declared) ---

	struct Quatf : public QuatBase<Quatf, float>
	{
		using QuatBase::QuatBase;
	};

	struct Quatd : public QuatBase<Quatd, double>
	{
		using QuatBase::QuatBase;
	};

#if defined(ROBOTICK_DEFAULT_REAL_IS_DOUBLE)
	struct Quat : public QuatBase<Quat, double>
	{
		using QuatBase::QuatBase;
	};
#else
	struct Quat : public QuatBase<Quat, float>
	{
		using QuatBase::QuatBase;
	};
#endif

} // namespace robotick
