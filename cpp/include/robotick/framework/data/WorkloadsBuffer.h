// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new> // operator new/delete with alignment
#include <stdexcept>

namespace robotick
{
	class RawBuffer
	{
	  public:
		RawBuffer() = default;
		RawBuffer(RawBuffer&&) noexcept = default;

		explicit RawBuffer(size_t size) : size(size) { allocate_aligned(size); }

		// delete all copy / move options - use create_mirror_from() instead
		RawBuffer(const RawBuffer&) = delete;
		RawBuffer& operator=(const RawBuffer&) = delete;

		// default move assignment
		RawBuffer& operator=(RawBuffer&&) noexcept = default;

		uint8_t* raw_ptr() { return data.get(); }
		const uint8_t* raw_ptr() const { return data.get(); }
		size_t get_size() const { return size; }

		bool contains_object(const uint8_t* query_ptr, const size_t query_size) const
		{
			const uint8_t* buffer_start = raw_ptr();
			const uint8_t* buffer_end = raw_ptr() + get_size();

			// Object lies entirely inside the buffer, inclusive of the last byte.
			// NB: Allow zero-sized objects while avoiding overflow.
			return (query_ptr >= buffer_start) && (query_size <= get_size()) && (query_ptr + query_size <= buffer_end);
		}

		bool contains_object(const void* query_ptr, const size_t query_size) const
		{
			return contains_object(static_cast<const uint8_t*>(query_ptr), query_size);
		}

		// One-time initialization only. Must be called once per RawBuffer instance.
		// Allocates memory to match source and copies its contents.
		void create_mirror_from(const RawBuffer& source)
		{
			if (data)
				ROBOTICK_ERROR("RawBuffer::create_mirror_from: buffer already allocated");

			size = source.size;
			allocate_aligned(size);
			update_mirror_from(source);
		}

		// Update this buffer with the contents of the source buffer.
		// Buffers must already be the same size — use create_mirror_from() to allocate initially.
		void update_mirror_from(const RawBuffer& source)
		{
			if (!data || size == 0)
				ROBOTICK_ERROR("RawBuffer::mirror_from: destination buffer not initialized");

			if (size != source.size)
				ROBOTICK_ERROR("RawBuffer::update_mirror_from: size mismatch");

			std::memcpy(data.get(), source.data.get(), size);
		}

		template <typename T> T* as(size_t offset = 0)
		{
			if (offset + sizeof(T) > size)
				ROBOTICK_ERROR("RawBuffer::as<T>: Offset out of range");

			if (offset % alignof(T) != 0)
				ROBOTICK_ERROR("RawBuffer::as<T>: Offset is not properly aligned for type T");

			uint8_t* ptr = data.get() + offset;
			return std::launder(reinterpret_cast<T*>(ptr));
		}

		template <typename T> const T* as(size_t offset = 0) const
		{
			if (offset + sizeof(T) > size)
				ROBOTICK_ERROR("RawBuffer::as<T>: Offset out of range");

			if (offset % alignof(T) != 0)
				ROBOTICK_ERROR("RawBuffer::as<T>: Offset is not properly aligned for type T");

			const uint8_t* ptr = data.get() + offset;
			return std::launder(reinterpret_cast<const T*>(ptr));
		}

	  private:
		size_t size = 0;

		struct AlignedDeleter
		{
			void operator()(void* ptr) const noexcept { ::operator delete(ptr, std::align_val_t{alignof(std::max_align_t)}); }
		};

		std::unique_ptr<uint8_t[], AlignedDeleter> data;

		void allocate_aligned(size_t alloc_size)
		{
			void* ptr = ::operator new(alloc_size, std::align_val_t{alignof(std::max_align_t)});
			data = std::unique_ptr<uint8_t[], AlignedDeleter>(static_cast<uint8_t*>(ptr));
		}
	};

	class WorkloadsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer; // inherit base-class constructors
	};

} // namespace robotick
