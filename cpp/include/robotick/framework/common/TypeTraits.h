// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>

namespace robotick
{
	template <typename T> using remove_reference_t = typename std::remove_reference<T>::type;
	template <typename T> using remove_cv_t = typename std::remove_cv<T>::type;

	template <typename... Args> using void_t = std::void_t<Args...>;

	template <typename T> using decay_t = typename std::decay<T>::type;

	template <typename T = void> using TrueType = std::true_type;
	template <typename T = void> using FalseType = std::false_type;

	template <typename T> using identity = T;
	template <typename T> T&& declval() noexcept;

	template <bool B, typename T = void> using enable_if_t = typename std::enable_if<B, T>::type;
} // namespace robotick
