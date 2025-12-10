// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <cstddef>
#include <new>

namespace robotick
{
	/// @brief A minimal fixed-size, heap-allocated vector.
	///
	/// `HeapVector<T>` provides a lightweight container that allocates memory
	/// once on initialization and forbids resizing or reallocation afterward.
	/// It supports copy/move construction and assignment only when the destination
	/// vector is uninitialized.
	///
	/// This is designed for embedded-safe or allocation-controlled environments
	/// like Robotick, where deterministic and non-fragmented memory usage is essential.
	///
	/// @tparam T Type of the stored elements.
	///
	/// @note Attempting to reinitialize, reassign, or resize will cause a fatal exit.
	///       Destruction calls all element destructors and frees memory.

	template <typename T> class HeapVector
	{
	  public:
		HeapVector() = default;

		/**
		 * @brief Construct from a fixed-size C-style array
		 * @tparam N Number of elements
		 * @param arr C-style array of T
		 */
		template <size_t N> explicit HeapVector(const T (&arr)[N])
		{
			initialize(N);
			for (size_t i = 0; i < N; ++i)
			{
				data()[i] = arr[i];
			}
		}

		~HeapVector() { destroy(); }

		// Copy constructor
		HeapVector(const HeapVector& other)
		{
			if (other.size_ == 0)
				return;
			data_ = static_cast<T*>(operator new[](other.size_ * sizeof(T)));
			size_t constructed = 0;
			try
			{
				for (; constructed < other.size_; ++constructed)
				{
					new (&data_[constructed]) T(other.data_[constructed]);
				}
				size_ = other.size_;
			}
			catch (...)
			{
				for (size_t i = 0; i < constructed; ++i)
				{
					data_[i].~T();
				}
				operator delete[](data_);
				data_ = nullptr;
				throw;
			}
		}

		// Copy assignment
		HeapVector& operator=(const HeapVector& other)
		{
			if (size_ > 0)
			{
				ROBOTICK_FATAL_EXIT("Cannot assign to already-initialized HeapVector");
			}
			if (other.size_ == 0)
				return *this;
			data_ = static_cast<T*>(operator new[](other.size_ * sizeof(T)));
			size_t constructed = 0;
			try
			{
				for (; constructed < other.size_; ++constructed)
				{
					new (&data_[constructed]) T(other.data_[constructed]);
				}
				size_ = other.size_;
			}
			catch (...)
			{
				for (size_t i = 0; i < constructed; ++i)
				{
					data_[i].~T();
				}
				operator delete[](data_);
				data_ = nullptr;
				throw;
			}
			return *this;
		}

		// Move constructor
		HeapVector(HeapVector&& other) noexcept
		{
			data_ = other.data_;
			size_ = other.size_;
			other.data_ = nullptr;
			other.size_ = 0;
		}

		// Move assignment
		HeapVector& operator=(HeapVector&& other) noexcept
		{
			if (size_ > 0)
			{
				ROBOTICK_FATAL_EXIT("Cannot move-assign to already-initialized HeapVector");
			}
			data_ = other.data_;
			size_ = other.size_;
			other.data_ = nullptr;
			other.size_ = 0;
			return *this;
		}

		void initialize(size_t count)
		{
			if (size_ > 0)
			{
				ROBOTICK_FATAL_EXIT("HeapVector::initialize() called more than once");
			}
			data_ = static_cast<T*>(operator new[](count * sizeof(T)));
			for (size_t i = 0; i < count; ++i)
			{
				new (&data_[i]) T();
			}
			size_ = count;
		}

		T* data() { return data_; }
		const T* data() const { return data_; }

		size_t size() const { return size_; }
		bool empty() const { return size_ == 0; }

		T& operator[](size_t index)
		{
			ROBOTICK_ASSERT_MSG(index < size_, "HeapVector index out of bounds [%zu/%zu]", index, size_);
			return data_[index];
		}

		const T& operator[](size_t index) const
		{
			ROBOTICK_ASSERT_MSG(index < size_, "HeapVector index out of bounds [%zu/%zu]", index, size_);
			return data_[index];
		}

		T* begin() { return data_; }
		T* end() { return data_ + size_; }
		const T* begin() const { return data_; }
		const T* end() const { return data_ + size_; }

	  private:
		void destroy()
		{
			if (data_)
			{
				for (size_t i = 0; i < size_; ++i)
				{
					data_[i].~T();
				}
				operator delete[](data_);
			}
			data_ = nullptr;
			size_ = 0;
		}

		T* data_ = nullptr;
		size_t size_ = 0;
	};
} // namespace robotick
