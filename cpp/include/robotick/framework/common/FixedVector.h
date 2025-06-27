

// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cassert>
#include <cstddef>

namespace robotick
{
	/**
	 * @brief A fixed-capacity vector container with no dynamic memory allocation.
	 *
	 * @tparam T Element type.
	 * @tparam Capacity Maximum number of elements that can be stored.
	 */
	template <typename T, size_t Capacity> class FixedVector
	{
	  public:
		/**
		 * @brief Returns the number of elements currently stored.
		 */
		constexpr size_t size() const { return count; }

		/**
		 * @brief Returns the maximum capacity of the vector.
		 */
		constexpr size_t capacity() const { return Capacity; }

		/**
		 * @brief Checks if the vector is empty.
		 */
		constexpr bool empty() const { return count == 0; }

		/**
		 * @brief Checks if the vector is full.
		 */
		constexpr bool full() const { return count == Capacity; }

		/**
		 * @brief Fills the vector to capacity, calling default constructor on any few elements
		 */
		void fill()
		{
			for (size_t i = count; i < Capacity; ++i)
			{
				data[i] = T{};
			}

			count = Capacity;
		}

		/**
		 * @brief Adds an element to the end of the vector.
		 *
		 * @param value The element to add.
		 */
		void add(const T& value)
		{
			assert(count < Capacity && "FixedVector overflow");
			data[count++] = value;
		}

		/**
		 * @brief Returns a reference to the element at the given index.
		 */
		T& operator[](size_t index)
		{
			assert(index < count);
			return data[index];
		}

		/**
		 * @brief Returns a const reference to the element at the given index.
		 */
		const T& operator[](size_t index) const
		{
			assert(index < count);
			return data[index];
		}

		/**
		 * @brief Returns a pointer to the first element.
		 */
		T* begin() { return data; }

		/**
		 * @brief Returns a pointer past the last element.
		 */
		T* end() { return data + count; }

		/**
		 * @brief Returns a const pointer to the first element.
		 */
		const T* begin() const { return data; }

		/**
		 * @brief Returns a const pointer past the last element.
		 */
		const T* end() const { return data + count; }

		/**
		 * @brief Clears all elements in the vector.
		 */
		void clear() { count = 0; }

	  private:
		T data[Capacity]{}; ///< Underlying storage.
		size_t count = 0;	///< Current number of elements.
	};

} // namespace robotick
