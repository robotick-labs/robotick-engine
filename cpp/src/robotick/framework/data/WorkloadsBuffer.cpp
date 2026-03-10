// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/WorkloadsBuffer.h"

#include <cstring>
#include <new> // operator new/delete with alignment

namespace robotick
{
	RawBuffer::RawBuffer(size_t size)
		: size(size)
	{
		allocate_aligned(size);
	}

	RawBuffer& RawBuffer::operator=(RawBuffer&& other) noexcept
	{
		if (this != &other)
		{
			size = other.size;
			data = robotick::move(other.data);
			other.size = 0;
		}
		return *this;
	}

	uint8_t* RawBuffer::raw_ptr()
	{
		return data.get();
	}

	const uint8_t* RawBuffer::raw_ptr() const
	{
		return data.get();
	}

	size_t RawBuffer::get_size() const
	{
		return size;
	}

	bool RawBuffer::contains_object(const uint8_t* query_ptr, const size_t query_size) const
	{
		const uint8_t* buffer_start = raw_ptr();
		const uint8_t* buffer_end = raw_ptr() + get_size();

		return (query_ptr >= buffer_start) && (query_size <= get_size()) && (query_ptr + query_size <= buffer_end);
	}

	bool RawBuffer::contains_object(const void* query_ptr, const size_t query_size) const
	{
		return contains_object(static_cast<const uint8_t*>(query_ptr), query_size);
	}

	void RawBuffer::create_mirror_from(const RawBuffer& source)
	{
		if (data.is_allocated())
			ROBOTICK_FATAL_EXIT("RawBuffer::create_mirror_from: buffer already allocated");

		size = source.size;
		allocate_aligned(size);
		update_mirror_from(source);
	}

	void RawBuffer::update_mirror_from(const RawBuffer& source)
	{
		if (!data.is_allocated() || size == 0)
			ROBOTICK_FATAL_EXIT("RawBuffer::mirror_from: destination buffer not initialized");

		if (size != source.size)
			ROBOTICK_FATAL_EXIT("RawBuffer::update_mirror_from: size mismatch");

		::memcpy(data.get(), source.data.get(), size);
	}

	RawBuffer::AlignedStorage::AlignedStorage(AlignedStorage&& other) noexcept
		: ptr(other.ptr)
		, size(other.size)
	{
		other.ptr = nullptr;
		other.size = 0;
	}

	RawBuffer::AlignedStorage& RawBuffer::AlignedStorage::operator=(AlignedStorage&& other) noexcept
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

	RawBuffer::AlignedStorage::~AlignedStorage()
	{
		release();
	}

	uint8_t* RawBuffer::AlignedStorage::get()
	{
		return ptr;
	}

	const uint8_t* RawBuffer::AlignedStorage::get() const
	{
		return ptr;
	}

	bool RawBuffer::AlignedStorage::is_allocated() const
	{
		return ptr != nullptr;
	}

	void RawBuffer::AlignedStorage::allocate(size_t alloc_size)
	{
		if (ptr != nullptr)
			ROBOTICK_FATAL_EXIT("AlignedStorage: attempt to allocate twice");

		void* raw = ::operator new(alloc_size, robotick::align_val_t{alignof(max_align_t)});
		::memset(raw, 0, alloc_size);
		ptr = static_cast<uint8_t*>(raw);
		size = alloc_size;
	}

	void RawBuffer::AlignedStorage::release()
	{
		if (ptr)
		{
			::operator delete(ptr, robotick::align_val_t{alignof(max_align_t)});
			ptr = nullptr;
			size = 0;
		}
	}

	void RawBuffer::allocate_aligned(size_t alloc_size)
	{
		data.allocate(alloc_size);
	}

	WorkloadsBuffer::WorkloadsBuffer(WorkloadsBuffer&& other) noexcept
		: RawBuffer(robotick::move(other))
		, size_used(other.size_used)
		, telemetry_frame_seq(other.telemetry_frame_seq.load())
	{
	}

	WorkloadsBuffer& WorkloadsBuffer::operator=(WorkloadsBuffer&& other) noexcept
	{
		if (this != &other)
		{
			RawBuffer::operator=(robotick::move(other));
			size_used = other.size_used;
			telemetry_frame_seq.store(other.telemetry_frame_seq.load());
		}
		return *this;
	}

	void WorkloadsBuffer::set_size_used(const size_t value)
	{
		size_used = value;
	}

	size_t WorkloadsBuffer::get_size_used() const
	{
		return size_used;
	}

	bool WorkloadsBuffer::contains_object_used_space(const uint8_t* query_ptr, const size_t query_size) const
	{
		const uint8_t* buffer_start = raw_ptr();
		const uint8_t* buffer_end = raw_ptr() + get_size_used();

		return (query_ptr >= buffer_start) && (query_size <= get_size_used()) && (query_ptr + query_size <= buffer_end);
	}

	bool WorkloadsBuffer::contains_object_used_space(const void* query_ptr, const size_t query_size) const
	{
		return contains_object_used_space(static_cast<const uint8_t*>(query_ptr), query_size);
	}

} // namespace robotick
