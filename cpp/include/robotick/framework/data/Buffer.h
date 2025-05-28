// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

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

		explicit RawBuffer(size_t size) : size(size), data(std::make_unique<char[]>(size)) {}

		RawBuffer(const RawBuffer& other) : size(other.size), data(std::make_unique<char[]>(other.size))
		{
			std::memcpy(data.get(), other.data.get(), size);
		}

		RawBuffer& operator=(const RawBuffer& other)
		{
			if (this != &other)
			{
				size = other.size;
				data = std::make_unique<char[]>(size);
				std::memcpy(data.get(), other.data.get(), size);
			}
			return *this;
		}

		void* raw_ptr() { return data.get(); }
		const void* raw_ptr() const { return data.get(); }
		size_t get_size() const { return size; }

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
			return reinterpret_cast<T*>(data.get() + offset);
		}

		template <typename T> const T* as(size_t offset = 0) const
		{
			if (offset + sizeof(T) > size)
				throw std::out_of_range("RawBuffer::as<T>: Offset out of range");
			return reinterpret_cast<const T*>(data.get() + offset);
		}

	  private:
		size_t size = 0;
		std::unique_ptr<char[]> data;
	};

	class BlackboardsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer;

		static BlackboardsBuffer& get_local_mirror();
		static const BlackboardsBuffer& get_source();
		static void set_source(const BlackboardsBuffer* buffer);

		void mirror_from_source();

	  private:
		static thread_local BlackboardsBuffer local_instance;
		static const BlackboardsBuffer* source_buffer;
	};

	class WorkloadsBuffer : public RawBuffer
	{
	  public:
		using RawBuffer::RawBuffer;

		static WorkloadsBuffer& get_local_mirror();
		static const WorkloadsBuffer& get_source();
		static void set_source(const WorkloadsBuffer* buffer);

		void mirror_from_source();

	  private:
		static thread_local WorkloadsBuffer local_instance;
		static const WorkloadsBuffer* source_buffer;
	};
} // namespace robotick
