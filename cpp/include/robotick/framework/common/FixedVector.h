

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
		 * @brief Fills the vector to capacity, calling default constructor on any new elements
		 */
		void fill()
		{
			for (size_t i = count; i < Capacity; ++i)
			{
				data_buffer[i] = T{};
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
			ROBOTICK_ASSERT(count < Capacity && "FixedVector overflow");
			data_buffer[count++] = value;
		}

		/**
		 * @brief Set the buffer contents from a raw memory block
		 *
		 * @param src Pointer to source data
		 * @param len Number of bytes to copy
		 */
		void set(const void* src, size_t len)
		{
			ROBOTICK_ASSERT(src != nullptr);
			ROBOTICK_ASSERT(len <= capacity());
			memcpy(data_buffer, src, len);
			count = len;
		}

		/**
		 * @brief Set the buffer-contents and size to the specified string
		 * (string-length plus null-terminator)
		 *
		 * @param value The string-value to set
		 */
		void set_from_string(const char* value)
		{
			ROBOTICK_ASSERT(value != nullptr);

			const size_t len = strlen(value) + 1; // include null terminator
			ROBOTICK_ASSERT(len <= capacity());

			memcpy(data_buffer, value, len);
			count = len;
		}

		/**
		 * @brief Appends a null-terminated string to the buffer.
		 *
		 * The null terminator is not included. Returns false if the buffer would overflow.
		 *
		 * @param cstr The string to append.
		 * @return true on success, false if the data would exceed capacity.
		 */
		bool append_from_string(const char* cstr)
		{
			if (!cstr)
				return false;

			size_t len = strlen(cstr);
			if (count + len > capacity())
				return false;

			memcpy(data() + count, cstr, len);
			count += len;
			return true;
		}

		/**
		 * @brief Appends formatted text to the buffer using printf-style syntax.
		 *
		 * Uses an internal 256-byte staging buffer. Returns false on formatting error or overflow.
		 *
		 * @param fmt The format string.
		 * @param ... Format arguments.
		 * @return true on success, false if the formatted string is too large or buffer would overflow.
		 */
		bool append_from_string_format(const char* fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			size_t available = capacity() - count;
			int written = std::vsnprintf(reinterpret_cast<char*>(data() + count), available, fmt, args);
			va_end(args);

			if (written <= 0 || static_cast<size_t>(written) >= available)
				return false;

			count += static_cast<size_t>(written);
			return true;
		}

		/**
		 * @brief Returns a reference to the element at the given index.
		 */
		T& operator[](size_t index)
		{
			ROBOTICK_ASSERT(index < count);
			return data_buffer[index];
		}

		/**
		 * @brief Returns a const reference to the element at the given index.
		 */
		const T& operator[](size_t index) const
		{
			ROBOTICK_ASSERT(index < count);
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

	  private:
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

} // namespace robotick
