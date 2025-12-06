// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <new>

#include "robotick/framework/memory/Memory.h"
#include "robotick/framework/utility/TypeTraits.h"

namespace robotick
{
	//------------------------------------------------------------------------------
	// Inline-state size threshold (in bytes)
	//------------------------------------------------------------------------------
	inline constexpr size_t MaxReasonableInlineStateSize = 5 * 1024; // 5 KB

	//------------------------------------------------------------------------------
	// State<T> - inline-only, destructor-safe state wrapper
	//------------------------------------------------------------------------------
	template <typename T> class State
	{
		static_assert(is_default_constructible_v<T>, "State<T> requires T to be default-constructible");

		static_assert(sizeof(T) <= MaxReasonableInlineStateSize,
			"State<T>: Type too large for inline storage; use StatePtr<T> instead. "
			"MaxReasonableInlineStateSize = 5120 bytes");

	  public:
		State() { new (&storage) T(); }
		State(const State&) = delete;
		State& operator=(const State&) = delete;

		State(State&& other) noexcept(is_nothrow_move_constructible_v<T>) { new (&storage) T(robotick::move(other.get())); }

		State& operator=(State&& other) noexcept(is_nothrow_move_constructible_v<T>)
		{
			if (this != &other)
			{
				get().~T();
				new (&storage) T(robotick::move(other.get()));
			}
			return *this;
		}

		~State() { get().~T(); }

		T* operator->() { return &get(); }
		const T* operator->() const { return &get(); }

		operator T&() { return get(); }
		operator const T&() const { return get(); }

		T& get() { return *reinterpret_cast<T*>(&storage); }
		const T& get() const { return *reinterpret_cast<const T*>(&storage); }

	  private:
		alignas(T) uint8_t storage[sizeof(T)];
	};

	//---------------------------------------------------------------------------------------------------
	// StatePtr<T> - pointer-based alternative for large states (allocated on heap once on startup)
	//---------------------------------------------------------------------------------------------------
	template <typename T, bool EnforceLargeState = true> class StatePtr
	{
		static_assert(is_default_constructible_v<T>, "StatePtr<T> requires T to be default-constructible");

		static_assert(!EnforceLargeState || sizeof(T) > MaxReasonableInlineStateSize,
			"StatePtr<T>: Type small enough for inline storage. "
			"Use State<T> instead.\n"
			"sizeof(T) = ??? (<=5KB threshold)");

	  public:
		StatePtr()
			: ptr(new T())
		{
		}
		~StatePtr() { destroy(); }

		// optional explicit destroy method - useful for controlling destruction order
		void destroy()
		{
			if (ptr)
			{
				delete ptr;
				ptr = nullptr;
			}
		}

		StatePtr(const StatePtr&) = delete;
		StatePtr& operator=(const StatePtr&) = delete;

		StatePtr(StatePtr&& other) noexcept
			: ptr(other.ptr)
		{
			other.ptr = nullptr;
		}

		StatePtr& operator=(StatePtr&& other) noexcept
		{
			if (this != &other)
			{
				delete ptr;
				ptr = other.ptr;
				other.ptr = nullptr;
			}
			return *this;
		}

		T* operator->() { return ptr; }
		const T* operator->() const { return ptr; }

		operator T&() { return *ptr; }
		operator const T&() const { return *ptr; }

		T& get() { return *ptr; }
		const T& get() const { return *ptr; }

	  private:
		T* ptr = nullptr;
	};

} // namespace robotick
