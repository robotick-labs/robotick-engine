// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/math/Quat.h"
#include "robotick/framework/math/Vec3.h"

namespace robotick
{
	// Transform is the engine's "how do I map one frame into another?" spatial type.
	//
	// Intended usage:
	// - frame-to-frame transforms, similar in spirit to ROS TF
	// - static sensor mounting transforms
	// - runtime kinematic or calibration transforms
	//
	// Semantics:
	// - `translation` is the offset from source frame origin to target frame origin
	// - `rotation` is the orientation change between those frames
	//
	// Transform vs Pose:
	// - Use Transform when the primary meaning is a relationship between frames.
	// - Use Pose when the primary meaning is the pose of an entity within a frame.
	// - Keeping both types makes model contracts clearer even though the math is closely related.

	namespace internal::Transform
	{
		// Match scalar type to the corresponding engine vector/quaternion type.
		template <typename T> struct VecSelector;
		template <> struct VecSelector<float>
		{
			using Type = Vec3f;
		};
		template <> struct VecSelector<double>
		{
			using Type = Vec3d;
		};

		template <typename T> struct QuatSelector;
		template <> struct QuatSelector<float>
		{
			using Type = Quatf;
		};
		template <> struct QuatSelector<double>
		{
			using Type = Quatd;
		};

		// Rotate a free vector by the transform rotation component.
		template <typename TReal>
		inline typename VecSelector<TReal>::Type rotate_vector(
			const typename QuatSelector<TReal>::Type& q, const typename VecSelector<TReal>::Type& v)
		{
			using TVec = typename VecSelector<TReal>::Type;
			using TQuat = typename QuatSelector<TReal>::Type;

			const TQuat pure(static_cast<TReal>(0), v.x, v.y, v.z);
			const TQuat rotated = q * pure * q.inverse();
			return TVec(rotated.x, rotated.y, rotated.z);
		}
	} // namespace internal::Transform

	// CRTP base shared by Transformf / Transformd.
	template <typename TDerived, typename TReal> struct TransformBase
	{
		using TElement = TReal;
		using TVec = typename internal::Transform::VecSelector<TReal>::Type;
		using TQuat = typename internal::Transform::QuatSelector<TReal>::Type;

		// Linear offset between frames.
		TVec translation;
		// Rotational offset between frames.
		TQuat rotation;

		TransformBase() = default;
		TransformBase(const TVec& translation, const TQuat& rotation)
			: translation(translation)
			, rotation(rotation)
		{
		}
		TransformBase(TReal x, TReal y, TReal z, TReal w, TReal qx, TReal qy, TReal qz)
			: translation(x, y, z)
			, rotation(w, qx, qy, qz)
		{
		}

		// Identity transform: no translation and no rotation.
		static TDerived identity() { return TDerived(TVec(), TQuat::identity()); }

		bool operator==(const TDerived& rhs) const { return translation == rhs.translation && rotation == rhs.rotation; }
		bool operator!=(const TDerived& rhs) const { return !(*this == rhs); }

		// Transform a point from local/source frame into the parent/target frame.
		TVec transform_point(const TVec& local_point) const { return translation + internal::Transform::rotate_vector<TReal>(rotation, local_point); }

		// Invert the transform so it maps in the opposite frame direction.
		TDerived inverse() const
		{
			const TQuat inv_rotation = rotation.inverse();
			const TVec inv_translation =
				internal::Transform::rotate_vector<TReal>(inv_rotation, TVec(-translation.x, -translation.y, -translation.z));
			return TDerived(inv_translation, inv_rotation);
		}

		// Compose transforms in rigid-body order.
		//
		// `a * b` means "apply b, then apply a".
		TDerived operator*(const TDerived& rhs) const { return TDerived(transform_point(rhs.translation), rotation * rhs.rotation); }
	};

	// Float and double specializations for explicit storage/control.
	//
	// Callers should choose `Transformf` or `Transformd` deliberately. We keep
	// the contract explicit so a build flag cannot silently change the scalar
	// width of a published transform type.
	struct Transformf : public TransformBase<Transformf, float>
	{
		using TransformBase::TransformBase;
	};

	struct Transformd : public TransformBase<Transformd, double>
	{
		using TransformBase::TransformBase;
	};

} // namespace robotick
