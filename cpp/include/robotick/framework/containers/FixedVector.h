// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

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
		 * @brief FixedVector constructor
		 * @param initial_size The size to initialize the vector to (defaults to zero)
		 */
		FixedVector(const size_t initial_size = 0)
		{
			ROBOTICK_ASSERT_MSG((initial_size <= capacity()), "FixedVector::FixedVector() - initial_size exceeds capacity");
			count = initial_size;
		}

		/**
		 * @brief Sets the current size of the vector. Use with care.
		 *        Caller must ensure the corresponding elements are initialized.
		 */
		void set_size(size_t new_size)
		{
			ROBOTICK_ASSERT_MSG((new_size <= capacity()), "FixedVector::set_size() exceeds capacity");
			count = new_size;
		}

		/**
		 * @brief Returns the number of elements currently stored.
		 */
		constexpr size_t size() const { return count; }

		/**
		 * @brief Returns the maximum capacity of the vector.
		 */
		static constexpr size_t capacity() { return Capacity; }

		/**
		 * @brief Checks if the vector is empty.
		 */
		constexpr bool empty() const { return count == 0; }

		/**
		 * @brief Checks if the vector is full.
		 */
		constexpr bool full() const { return count == Capacity; }

		/**
		 * @brief Fills the vector to capacity, starting at current element
		 */
		void fill(const T value = T{})
		{
			for (size_t i = 0; i < Capacity; ++i)
			{
				data_buffer[i] = value;
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
			ROBOTICK_ASSERT_MSG(count < Capacity, "FixedVector::add() - overflow");
			data_buffer[count++] = value;
		}

		/**
		 * @brief Set the buffer contents from an array of elements.
		 *
		 * @param src Pointer to the source array of elements
		 * @param len Number of elements to copy
		 */
		void set(const T* src, size_t len)
		{
			ROBOTICK_ASSERT(src != nullptr);
			ROBOTICK_ASSERT(len <= capacity());
			memcpy(data_buffer, src, len * sizeof(T));
			count = len;
		}

		/**
		 * @brief Set the buffer contents from a raw byte block.
		 *
		 * @param src Pointer to raw memory block
		 * @param byte_count Number of bytes to copy (must be divisible by sizeof(T))
		 */
		void set_bytes(const void* src, size_t const byte_count)
		{
			const size_t element_count = byte_count / sizeof(T);
			ROBOTICK_ASSERT(src != nullptr);
			ROBOTICK_ASSERT_MSG(
				element_count <= capacity(), "Too many elements (%zu - %zu bytes) provided - capacity is %zu", element_count, byte_count, capacity());
			memcpy(data_buffer, src, byte_count);
			count = element_count;
		}

		/**
		 * @brief Returns a reference to the element at the given index.
		 */
		T& operator[](size_t index)
		{
			ROBOTICK_ASSERT_MSG(index < Capacity, "Indexing beyond Capacity (non-const [] accessor)");
			ROBOTICK_ASSERT_MSG(index < count, "Indexing beyond current size of %zu (non-const [] accessor)", static_cast<size_t>(count));
			return data_buffer[index];
		}

		/**
		 * @brief Returns a const reference to the element at the given index.
		 */
		const T& operator[](size_t index) const
		{
			ROBOTICK_ASSERT_MSG(index < Capacity, "Indexing beyond Capacity (const [] accessor)");
			ROBOTICK_ASSERT_MSG(index < count, "Indexing beyond current size of %zu (const [] accessor)", static_cast<size_t>(count));
			return data_buffer[index];
		}

		/**
		 * @brief Returns a pointer to the first element.
		 */
		T* begin() { return data_buffer; }

		/**
		 * @brief Returns a pointer past the last element.
		 */
		T* end() { return data_buffer + count; }

		/**
		 * @brief Returns a const pointer to the first element.
		 */
		const T* begin() const { return data_buffer; }

		/**
		 * @brief Returns a const pointer past the last element.
		 */
		const T* end() const { return data_buffer + count; }

		/**
		 * @brief Clears all elements in the vector.
		 */
		void clear() { count = 0; }

		/**
		 * @brief Returns pointer to start of our data-buffer
		 */
		T* data() { return &data_buffer[0]; };

		/**
		 * @brief Returns const pointer to start of our data-buffer
		 */
		const T* data() const { return &data_buffer[0]; };

	  public:
		T data_buffer[Capacity]{}; ///< Underlying storage.
		uint32_t count = 0;		   ///< Current number of elements.
	};

	using FixedVector1k = FixedVector<uint8_t, 1 * 1024>;
	using FixedVector2k = FixedVector<uint8_t, 2 * 1024>;
	using FixedVector4k = FixedVector<uint8_t, 4 * 1024>;
	using FixedVector8k = FixedVector<uint8_t, 8 * 1024>;
	using FixedVector16k = FixedVector<uint8_t, 16 * 1024>;
	using FixedVector32k = FixedVector<uint8_t, 32 * 1024>;
	using FixedVector64k = FixedVector<uint8_t, 64 * 1024>;
	using FixedVector128k = FixedVector<uint8_t, 128 * 1024>;
	using FixedVector256k = FixedVector<uint8_t, 256 * 1024>;

	// Reusable macro for registering a FixedVector<T, N> style struct
	// 	usage: ROBOTICK_REGISTER_FIXED_VECTOR(MyVecType, element_type)

#define ROBOTICK_REGISTER_FIXED_VECTOR(StructType, ElementType)                                                                                      \
	ROBOTICK_REGISTER_STRUCT_BEGIN(StructType)                                                                                                       \
	ROBOTICK_STRUCT_FIXED_ARRAY_FIELD(StructType, ElementType, StructType::capacity(), data_buffer)                                                  \
	ROBOTICK_STRUCT_FIELD(StructType, uint32_t, count)                                                                                               \
	ROBOTICK_REGISTER_STRUCT_END(StructType)

} // namespace robotick
