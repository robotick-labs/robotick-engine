// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/NoStl.h"

#include <new>
#include <utility>

namespace robotick
{
	template <typename T>
	constexpr typename std_approved::remove_reference<T>::type&& move(T&& value) noexcept
	{
		return std_approved::move(value);
	}

	template <typename T>
	constexpr T&& forward(typename std_approved::remove_reference<T>::type& value) noexcept
	{
		return std_approved::forward<T>(value);
	}

	template <typename T>
	constexpr T&& forward(typename std_approved::remove_reference<T>::type&& value) noexcept
	{
		return std_approved::forward<T>(std_approved::move(value));
	}

	template <typename T>
	constexpr T* launder(T* ptr) noexcept
	{
		return std_approved::launder(ptr);
	}

	template <typename T>
	constexpr const T* launder(const T* ptr) noexcept
	{
		return std_approved::launder(ptr);
	}

	using align_val_t = std_approved::align_val_t;
} // namespace robotick
