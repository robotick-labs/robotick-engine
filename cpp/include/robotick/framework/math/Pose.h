// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/math/Quat.h"
#include "robotick/framework/math/Vec3.h"

namespace robotick
{
	// Pose is the engine's "where is this thing?" spatial type.
	//
	// Intended usage:
	// - estimated robot pose in a world/map frame
	// - remembered object or anchor pose
	// - sensor or body-part pose relative to some parent frame
	//
	// Semantics:
	// - `position` is the location of the frame origin
	// - `orientation` is the frame rotation
	//
	// Pose vs Transform:
	// - Use Pose when the important meaning is "the pose of an entity".
	// - Use Transform when the important meaning is "the mapping from one frame to another".
	// - The math is intentionally similar, but the semantic distinction matters in model APIs.
	//
	// Representation:
	// - orientation is stored as a quaternion, not Euler angles
	// - downstream code can derive yaw/pitch/roll when needed, but quaternion remains the primary contract

	namespace internal::Pose
	{
		// These selectors mirror the pattern used by other engine math types:
		// the templated base works in terms of a scalar type, while the concrete
		// storage types resolve to the matching Vec3/Quat specializations.
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

		// Rotate a free vector by a quaternion.
		//
		// This helper is used by Pose operations such as point transformation and
		// inversion. It deliberately treats the vector as a pure quaternion so the
		// translation part of a pose never leaks into this step.
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
	} // namespace internal::Pose

	// CRTP base shared by Posef / Posed.
	//
	// This follows the same pattern as Vec and Quat: the generic implementation
	// lives here, while the concrete leaf types remain trivial standard-layout
	// structs that can be registered with the engine type system.
	template <typename TDerived, typename TReal> struct PoseBase
	{
		using TElement = TReal;
		using TVec = typename internal::Pose::VecSelector<TReal>::Type;
		using TQuat = typename internal::Pose::QuatSelector<TReal>::Type;

		// Position of the pose origin in the parent/reference frame.
		TVec position;
		// Orientation of the pose frame relative to the parent/reference frame.
		TQuat orientation;

		PoseBase() = default;
		PoseBase(const TVec& position, const TQuat& orientation)
			: position(position)
			, orientation(orientation)
		{
		}
		PoseBase(TReal x, TReal y, TReal z, TReal w, TReal qx, TReal qy, TReal qz)
			: position(x, y, z)
			, orientation(w, qx, qy, qz)
		{
		}

		// Identity pose: zero translation and identity rotation.
		static TDerived identity() { return TDerived(TVec(), TQuat::identity()); }

		bool operator==(const TDerived& rhs) const { return position == rhs.position && orientation == rhs.orientation; }
		bool operator!=(const TDerived& rhs) const { return !(*this == rhs); }

		// Transform a point expressed in the pose-local frame into the parent frame.
		TVec transform_point(const TVec& local_point) const { return position + internal::Pose::rotate_vector<TReal>(orientation, local_point); }

		// Invert the pose so it maps parent-frame points back into the pose-local frame.
		TDerived inverse() const
		{
			const TQuat inv_rotation = orientation.inverse();
			const TVec inv_translation = internal::Pose::rotate_vector<TReal>(inv_rotation, TVec(-position.x, -position.y, -position.z));
			return TDerived(inv_translation, inv_rotation);
		}

		// Compose poses in the usual rigid-body sense.
		//
		// `a * b` means "apply b in a's frame, then express the result in a's parent frame".
		TDerived operator*(const TDerived& rhs) const { return TDerived(transform_point(rhs.position), orientation * rhs.orientation); }
	};

	// Float and double specializations for explicit storage/control.
	//
	// Callers should choose `Posef` or `Posed` deliberately. We intentionally
	// avoid a build-time default-real alias here so model contracts and
	// registered schemas remain unambiguous.
	struct Posef : public PoseBase<Posef, float>
	{
		using PoseBase::PoseBase;
	};

	struct Posed : public PoseBase<Posed, double>
	{
		using PoseBase::PoseBase;
	};

} // namespace robotick
