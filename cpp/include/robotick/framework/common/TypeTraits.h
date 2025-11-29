// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/NoStl.h"
#include <type_traits>

namespace robotick
{
	template <typename T> using remove_reference_t = typename std_approved::remove_reference<T>::type;
	template <typename T> using remove_cv_t = typename std_approved::remove_cv<T>::type;

	template <typename... Args> using void_t = std_approved::void_t<Args...>;

	template <typename T> using decay_t = typename std_approved::decay<T>::type;

	template <typename T = void> using TrueType = std_approved::true_type;
	template <typename T = void> using FalseType = std_approved::false_type;

	template <typename T> using identity = T;
	template <typename T> T&& declval() noexcept;

	template <bool B, typename T = void> using enable_if_t = typename std_approved::enable_if<B, T>::type;

	template <typename T> constexpr bool is_standard_layout_v = std_approved::is_standard_layout<T>::value;
	template <typename T> constexpr bool is_trivially_copyable_v = std_approved::is_trivially_copyable<T>::value;
} // namespace robotick
