// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/memory/Memory.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new> // operator new/delete with alignment
#include <stdexcept>

namespace robotick
{

	class RawBuffer
	{
	  public:
		RawBuffer() = default;
		RawBuffer(RawBuffer&&) noexcept = default;

		explicit RawBuffer(size_t size)
			: size(size)
		{
			allocate_aligned(size);
		}

		RawBuffer(const RawBuffer&) = delete;
		RawBuffer& operator=(const RawBuffer&) = delete;

		RawBuffer& operator=(RawBuffer&& other) noexcept
		{
			if (this != &other)
			{
				size = other.size;
				data = robotick::move(other.data);
				other.size = 0;
			}
			return *this;
		}

		uint8_t* raw_ptr() { return data.get(); }
		const uint8_t* raw_ptr() const { return data.get(); }
		size_t get_size() const { return size; }

		bool contains_object(const uint8_t* query_ptr, const size_t query_size) const
		{
			const uint8_t* buffer_start = raw_ptr();
			const uint8_t* buffer_end = raw_ptr() + get_size();

			return (query_ptr >= buffer_start) && (query_size <= get_size()) && (query_ptr + query_size <= buffer_end);
		}

		bool contains_object(const void* query_ptr, const size_t query_size) const
		{
			return contains_object(static_cast<const uint8_t*>(query_ptr), query_size);
		}

		void create_mirror_from(const RawBuffer& source)
		{
			if (data.is_allocated())
				ROBOTICK_FATAL_EXIT("RawBuffer::create_mirror_from: buffer already allocated");

			size = source.size;
			allocate_aligned(size);
			update_mirror_from(source);
		}

		void update_mirror_from(const RawBuffer& source)
		{
			if (!data.is_allocated() || size == 0)
				ROBOTICK_FATAL_EXIT("RawBuffer::mirror_from: destination buffer not initialized");

			if (size != source.size)
				ROBOTICK_FATAL_EXIT("RawBuffer::update_mirror_from: size mismatch");

			::memcpy(data.get(), source.data.get(), size);
		}

		template <typename T> T* as(size_t offset = 0)
		{
			if (offset + sizeof(T) > size)
				ROBOTICK_FATAL_EXIT("RawBuffer::as<T>: Offset out of range");

			if (offset % alignof(T) != 0)
				ROBOTICK_FATAL_EXIT("RawBuffer::as<T>: Offset is not properly aligned for type T");

			uint8_t* ptr = data.get() + offset;
			return robotick::launder(reinterpret_cast<T*>(ptr));
		}

		template <typename T> const T* as(size_t offset = 0) const
		{
			if (offset + sizeof(T) > size)
				ROBOTICK_FATAL_EXIT("RawBuffer::as<T>: Offset out of range");

			if (offset % alignof(T) != 0)
				ROBOTICK_FATAL_EXIT("RawBuffer::as<T>: Offset is not properly aligned for type T");

			const uint8_t* ptr = data.get() + offset;
			return robotick::launder(reinterpret_cast<const T*>(ptr));
		}

	  private:
		size_t size = 0;

		struct AlignedStorage
		{
			AlignedStorage() = default;
			AlignedStorage(AlignedStorage&& other) noexcept
				: ptr(other.ptr)
				, size(other.size)
			{
				other.ptr = nullptr;
				other.size = 0;
			}

			AlignedStorage& operator=(AlignedStorage&& other) noexcept
			{
				if (this != &other)
				{
					release();
					ptr = other.ptr;
					size = other.size;
					other.ptr = nullptr;
					other.size = 0;
				}
				return *this;
			}

			AlignedStorage(const AlignedStorage&) = delete;
			AlignedStorage& operator=(const AlignedStorage&) = delete;

			~AlignedStorage() { release(); }

			uint8_t* get() { return ptr; }
			const uint8_t* get() const { return ptr; }

			bool is_allocated() const { return ptr != nullptr; }

			void allocate(size_t alloc_size)
			{
				if (ptr != nullptr)
					ROBOTICK_FATAL_EXIT("AlignedStorage: attempt to allocate twice");

				void* raw = ::operator new(alloc_size, robotick::align_val_t{alignof(max_align_t)});
				::memset(raw, 0, alloc_size);
				ptr = static_cast<uint8_t*>(raw);
				size = alloc_size;
			}

			void release()
			{
				if (ptr)
				{
					::operator delete(ptr, robotick::align_val_t{alignof(max_align_t)});
					ptr = nullptr;
					size = 0;
				}
			}

			uint8_t* ptr = nullptr;
			size_t size = 0;
		};

		AlignedStorage data;

		void allocate_aligned(size_t alloc_size) { data.allocate(alloc_size); }
	};

	class WorkloadsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer;

		void set_size_used(const size_t value) { size_used = value; }
		size_t get_size_used() const { return size_used; };

		bool contains_object_used_space(const uint8_t* query_ptr, const size_t query_size) const
		{
			const uint8_t* buffer_start = raw_ptr();
			const uint8_t* buffer_end = raw_ptr() + get_size_used();

			return (query_ptr >= buffer_start) && (query_size <= get_size_used()) && (query_ptr + query_size <= buffer_end);
		}

		bool contains_object_used_space(const void* query_ptr, const size_t query_size) const
		{
			return contains_object_used_space(static_cast<const uint8_t*>(query_ptr), query_size);
		}

	  private:
		size_t size_used = 0;
	};

} // namespace robotick
