// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace robotick
{
	class RawBuffer
	{
	  public:
		RawBuffer() = default;
		RawBuffer(RawBuffer&&) noexcept = default;

		explicit RawBuffer(size_t size) : size(size), data(std::make_unique<uint8_t[]>(size)) {}

		RawBuffer(const RawBuffer& other) : size(other.size), data(std::make_unique<uint8_t[]>(other.size))
		{
			std::memcpy(data.get(), other.data.get(), size);
		}

		RawBuffer& operator=(RawBuffer&&) noexcept = default;

		RawBuffer& operator=(const RawBuffer& other)
		{
			if (this != &other)
			{
				size = other.size;
				if (size != other.size)
				{
					data = std::make_unique<uint8_t[]>(other.size);
				}
				std::memcpy(data.get(), other.data.get(), size);
			}
			return *this;
		}

		uint8_t* raw_ptr() { return data.get(); }
		const uint8_t* raw_ptr() const { return data.get(); }
		size_t get_size() const { return size; }

		bool is_within_buffer(const uint8_t* query_ptr) const { return (query_ptr >= raw_ptr()) && (query_ptr < raw_ptr() + get_size()); }

		bool is_within_buffer(void* query_ptr) const { return is_within_buffer((uint8_t*)query_ptr); };

		void mirror_from(const RawBuffer& source)
		{
			if (size != source.size)
				throw std::runtime_error("RawBuffer::mirror_from: size mismatch");
			std::memcpy(data.get(), source.data.get(), size);
		}

		template <typename T> T* as(size_t offset = 0)
		{
			if (offset + sizeof(T) > size)
				throw std::out_of_range("RawBuffer::as<T>: Offset out of range");

			uint8_t* ptr = data.get() + offset;
			assert(reinterpret_cast<std::uintptr_t>(ptr) % alignof(T) == 0 && "Misaligned field offset for type T");
			return std::launder(reinterpret_cast<T*>(ptr));
		}

		template <typename T> const T* as(size_t offset = 0) const
		{
			if (offset + sizeof(T) > size)
				throw std::out_of_range("RawBuffer::as<T>: Offset out of range");

			uint8_t* ptr = data.get() + offset;
			assert(reinterpret_cast<std::uintptr_t>(ptr) % alignof(T) == 0 && "Misaligned field offset for type T");
			return std::launder(reinterpret_cast<T*>(ptr));
		}

	  private:
		size_t size = 0;
		std::unique_ptr<uint8_t[]> data;
	};

	class BlackboardsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer;

		static BlackboardsBuffer& get_source();
		static void set_source(BlackboardsBuffer* buffer);

		void mirror_from_source();

	  private:
		static thread_local BlackboardsBuffer local_instance;
		static BlackboardsBuffer* source_buffer;
	};

	class WorkloadsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer;

		static WorkloadsBuffer& get_source();
		static void set_source(WorkloadsBuffer* buffer);

		void mirror_from_source();

	  private:
		static WorkloadsBuffer* source_buffer;
	};
} // namespace robotick
