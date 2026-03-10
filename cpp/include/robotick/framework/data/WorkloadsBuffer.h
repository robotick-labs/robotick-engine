// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/memory/Memory.h"

#include <cstddef>
#include <cstdint>

namespace robotick
{

	class RawBuffer
	{
	  public:
		RawBuffer() = default;
		RawBuffer(RawBuffer&&) noexcept = default;

		explicit RawBuffer(size_t size);

		RawBuffer(const RawBuffer&) = delete;
		RawBuffer& operator=(const RawBuffer&) = delete;

		RawBuffer& operator=(RawBuffer&& other) noexcept;

		uint8_t* raw_ptr();
		const uint8_t* raw_ptr() const;
		size_t get_size() const;

		bool contains_object(const uint8_t* query_ptr, const size_t query_size) const;

		bool contains_object(const void* query_ptr, const size_t query_size) const;

		void create_mirror_from(const RawBuffer& source);

		void update_mirror_from(const RawBuffer& source);

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
			AlignedStorage(AlignedStorage&& other) noexcept;

			AlignedStorage& operator=(AlignedStorage&& other) noexcept;

			AlignedStorage(const AlignedStorage&) = delete;
			AlignedStorage& operator=(const AlignedStorage&) = delete;

			~AlignedStorage();

			uint8_t* get();
			const uint8_t* get() const;

			bool is_allocated() const;

			void allocate(size_t alloc_size);
			void release();

			uint8_t* ptr = nullptr;
			size_t size = 0;
		};

		AlignedStorage data;

		void allocate_aligned(size_t alloc_size);
	};

	class WorkloadsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer;

		WorkloadsBuffer() = default;

		WorkloadsBuffer(WorkloadsBuffer&& other) noexcept;

		WorkloadsBuffer& operator=(WorkloadsBuffer&& other) noexcept;

		void set_size_used(const size_t value);
		size_t get_size_used() const;

		bool contains_object_used_space(const uint8_t* query_ptr, const size_t query_size) const;

		bool contains_object_used_space(const void* query_ptr, const size_t query_size) const;

		// Telemetry frame sequence (seqlock-style):
		// odd = write in progress, even = stable snapshot.
		inline void mark_frame_write_begin()
		{
			uint32_t seq = telemetry_frame_seq.load();
			if ((seq & 1u) == 0u)
			{
				telemetry_frame_seq.store(seq + 1u);
				return;
			}

			// Already odd (unexpected but safe): move to next odd value.
			telemetry_frame_seq.store(seq + 2u);
		}

		inline void mark_frame_write_end()
		{
			uint32_t seq = telemetry_frame_seq.load();
			if ((seq & 1u) != 0u)
			{
				telemetry_frame_seq.store(seq + 1u);
				return;
			}

			// Already even (unexpected but safe): move to next even value.
			telemetry_frame_seq.store(seq + 2u);
		}

		inline uint32_t get_telemetry_frame_seq() const { return telemetry_frame_seq.load(); }

	  private:
		size_t size_used = 0;
		AtomicValue<uint32_t> telemetry_frame_seq{0};
	};

} // namespace robotick
