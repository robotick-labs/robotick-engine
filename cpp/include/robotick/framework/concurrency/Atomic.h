// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <type_traits>

namespace robotick
{
	class AtomicFlag
	{
	  public:
		explicit AtomicFlag(bool initial = false)
			: flag(initial)
		{
		}

		inline void set(bool value = true) { flag.store(value); }
		inline void unset() { flag.store(false); }
		inline bool is_set() const { return flag.load(); }

	  private:
		std_approved::atomic<bool> flag{false};
	};

	template <typename T> class AtomicValue
	{
	  public:
		explicit AtomicValue(T initial = T())
			: value(initial)
		{
		}

		inline void store(T v, std_approved::memory_order order = std_approved::memory_order_seq_cst) { value.store(v, order); }

		inline T load(std_approved::memory_order order = std_approved::memory_order_seq_cst) const { return value.load(order); }

		inline T exchange(T desired, std_approved::memory_order order = std_approved::memory_order_seq_cst) { return value.exchange(desired, order); }

		inline bool compare_exchange_weak(T& expected, T desired, std_approved::memory_order success, std_approved::memory_order failure)
		{
			return value.compare_exchange_weak(expected, desired, success, failure);
		}

		inline bool compare_exchange_weak(T& expected, T desired) { return value.compare_exchange_weak(expected, desired); }

		inline bool compare_exchange_strong(T& expected, T desired, std_approved::memory_order success, std_approved::memory_order failure)
		{
			return value.compare_exchange_strong(expected, desired, success, failure);
		}

		inline bool compare_exchange_strong(T& expected, T desired) { return value.compare_exchange_strong(expected, desired); }

		inline T fetch_add(T arg, std_approved::memory_order order = std_approved::memory_order_seq_cst)
		{
			static_assert(std_approved::is_integral<T>::value, "fetch_add only valid for integral types");
			return value.fetch_add(arg, order);
		}

		inline T fetch_sub(T arg, std_approved::memory_order order = std_approved::memory_order_seq_cst)
		{
			static_assert(std_approved::is_integral<T>::value, "fetch_sub only valid for integral types");
			return value.fetch_sub(arg, order);
		}

		inline T fetch_or(T arg, std_approved::memory_order order = std_approved::memory_order_seq_cst)
		{
			static_assert(std_approved::is_integral<T>::value, "fetch_or only valid for integral types");
			return value.fetch_or(arg, order);
		}

		inline T fetch_and(T arg, std_approved::memory_order order = std_approved::memory_order_seq_cst)
		{
			static_assert(std_approved::is_integral<T>::value, "fetch_and only valid for integral types");
			return value.fetch_and(arg, order);
		}

		inline T fetch_xor(T arg, std_approved::memory_order order = std_approved::memory_order_seq_cst)
		{
			static_assert(std_approved::is_integral<T>::value, "fetch_xor only valid for integral types");
			return value.fetch_xor(arg, order);
		}

		inline T operator++()
		{
			static_assert(std_approved::is_integral<T>::value, "increment only valid for integral types");
			return value.fetch_add(1) + 1;
		}

		inline T operator--()
		{
			static_assert(std_approved::is_integral<T>::value, "decrement only valid for integral types");
			return value.fetch_sub(1) - 1;
		}

		inline T operator++(int)
		{
			static_assert(std_approved::is_integral<T>::value, "increment only valid for integral types");
			return value.fetch_add(1);
		}

		inline T operator--(int)
		{
			static_assert(std_approved::is_integral<T>::value, "decrement only valid for integral types");
			return value.fetch_sub(1);
		}

	  private:
		std_approved::atomic<T> value;
	};

	// Lightweight wrappers around std_approved::atomic_thread_fence so call sites do not pull in <atomic> directly.
	// These belong in Atomic.h (rather than Thread.h) because they relate to memory ordering, not scheduling.
	inline void thread_fence(std_approved::memory_order order)
	{
		std_approved::atomic_thread_fence(order);
	}

	inline void thread_fence_release()
	{
		std_approved::atomic_thread_fence(std_approved::memory_order_release);
	}

	inline void thread_fence_acquire()
	{
		std_approved::atomic_thread_fence(std_approved::memory_order_acquire);
	}

} // namespace robotick
