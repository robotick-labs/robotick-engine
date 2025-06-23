// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/HeapVector.h"
#endif // #ifdef ROBOTICK_ENABLE_MODEL_HEAP

#include <cstddef> // size_t

namespace robotick
{
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

		/// @brief Construct from pointer and size
		constexpr ArrayView(T* data_in, size_t size_in) : data(data_in), array_size(size_in) {}

		/// @brief Construct from fixed-size C-style array
		template <size_t N> constexpr ArrayView(T (&arr)[N]) : data(arr), array_size(N) {}

		/// @brief Assign from pointer and size
		void use(T* data_in, size_t size_in)
		{
			if (size_in > 0 && data_in == nullptr)
				ROBOTICK_FATAL_EXIT("ArrayView::use called with null data and non-zero size");

			data = data_in;
			array_size = size_in;
		}

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		/// @brief Assign from HeapVector
		void use(HeapVector<T>& source_in)
		{
			data = source_in.data();
			array_size = source_in.size();
		}
#endif // #ifdef ROBOTICK_ENABLE_MODEL_HEAP

		/// @brief Access raw pointer to data
		T* data_ptr() { return data; }
		const T* data_ptr() const { return data; }

		/// @brief Get number of elements in view
		size_t size() const { return array_size; }
		bool empty() const { return array_size == 0; }

		/// @brief Bounds-checked element access
		T& operator[](size_t index)
		{
			if (index >= array_size)
				ROBOTICK_FATAL_EXIT("ArrayView index out of bounds");
			return data[index];
		}

		const T& operator[](size_t index) const
		{
			if (index >= array_size)
				ROBOTICK_FATAL_EXIT("ArrayView index out of bounds");
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
