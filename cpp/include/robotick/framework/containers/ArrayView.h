// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <cstddef> // size_t

namespace robotick
{
	namespace detail
	{
		template <bool Condition, typename T = void> struct EnableIf
		{
		};

		template <typename T> struct EnableIf<true, T>
		{
			using type = T;
		};

		template <typename T> struct IsConst
		{
			static constexpr bool value = false;
		};

		template <typename T> struct IsConst<const T>
		{
			static constexpr bool value = true;
		};
	} // namespace detail

	/**
	 * @brief A non-owning view of a fixed-size array.
	 *
	 * `ArrayView<T>` wraps a `T*` and a size, providing bounds-checked access
	 * and iteration. It does not own memory or perform allocation.
	 * Useful for APIs that operate on slices of data without copying.
	 *
	 * @tparam T Type of the referenced elements.
	 */
	template <typename T> class ArrayView
	{
	  public:
		/// @brief Default constructor: empty view
		constexpr ArrayView() = default;

		/// @brief Copy constructor
		constexpr ArrayView(const ArrayView& other) = default;

		/// @brief Construct from pointer and size
		ArrayView(T* data_in, size_t size_in)
			: data(data_in)
			, array_size(size_in)
		{
			if (size_in > 0 && data_in == nullptr)
				ROBOTICK_FATAL_EXIT("ArrayView::use called with null data and non-zero size");
		}

		/// @brief Construct from fixed-size C-style array
		template <size_t N>
		constexpr ArrayView(T (&arr)[N])
			: data(arr)
			, array_size(N)
		{
		}

		template <size_t N, typename Dummy = T, typename detail::EnableIf<!detail::IsConst<Dummy>::value, int>::type = 0>
		constexpr ArrayView(const T (&arr)[N])
			: data(const_cast<T*>(arr))
			, array_size(N)
		{
		}

		/// @brief Assignment operator
		constexpr ArrayView& operator=(const ArrayView& other) = default;

		/// @brief Assign from pointer and size
		void use(T* data_in, size_t size_in)
		{
			if (size_in > 0 && data_in == nullptr)
				ROBOTICK_FATAL_EXIT("ArrayView::use called with null data and non-zero size");

			data = data_in;
			array_size = size_in;
		}

		/// @brief Access raw pointer to data
		T* data_ptr() { return data; }
		const T* data_ptr() const { return data; }

		/// @brief Get number of elements in view
		size_t size() const { return array_size; }
		bool empty() const { return array_size == 0; }

		/// @brief Debug-only bounds assertions on element access
		T& operator[](size_t index)
		{
			ROBOTICK_ASSERT_MSG((index < array_size), "ArrayView index out of bounds [%zu/%zu]", index, array_size);
			return data[index];
		}

		const T& operator[](size_t index) const
		{
			ROBOTICK_ASSERT_MSG((index < array_size), "ArrayView index out of bounds [%zu/%zu]", index, array_size);
			return data[index];
		}

		/// @brief Iterator support
		T* begin() { return data; }
		T* end() { return data + array_size; }
		const T* begin() const { return data; }
		const T* end() const { return data + array_size; }

	  private:
		T* data = nullptr;
		size_t array_size = 0;
	};
} // namespace robotick
