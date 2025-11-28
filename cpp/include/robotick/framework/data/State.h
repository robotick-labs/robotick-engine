// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

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
		static_assert(std::is_default_constructible<T>::value, "State<T> requires T to be default-constructible");

		static_assert(sizeof(T) <= MaxReasonableInlineStateSize,
			"State<T>: Type too large for inline storage; use StatePtr<T> instead. "
			"MaxReasonableInlineStateSize = 5120 bytes");

	  public:
		State() { new (&storage) T(); }
		State(const State&) = delete;
		State& operator=(const State&) = delete;

		State(State&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
		{
			new (&storage) T(std::move(other.get()));
		}

		State& operator=(State&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
		{
			if (this != &other)
			{
				get().~T();
				new (&storage) T(std::move(other.get()));
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

	//------------------------------------------------------------------------------
	// StatePtr<T> - pointer-based alternative for large states
	//------------------------------------------------------------------------------
	template <typename T> class StatePtr
	{
		static_assert(std::is_default_constructible<T>::value, "StatePtr<T> requires T to be default-constructible");

		static_assert(sizeof(T) > MaxReasonableInlineStateSize,
			"StatePtr<T>: Type small enough for inline storage. "
			"Use State<T> instead.\n"
			"sizeof(T) = ??? (<=5KB threshold)");

	  public:
		StatePtr() : ptr(std::make_unique<T>()) {}
		~StatePtr() = default;

		StatePtr(const StatePtr&) = delete;
		StatePtr& operator=(const StatePtr&) = delete;
		StatePtr(StatePtr&&) noexcept = default;
		StatePtr& operator=(StatePtr&&) noexcept = default;

		T* operator->() { return ptr.get(); }
		const T* operator->() const { return ptr.get(); }

		operator T&() { return *ptr; }
		operator const T&() const { return *ptr; }

		T& get() { return *ptr; }
		const T& get() const { return *ptr; }

	  private:
		std::unique_ptr<T> ptr;
	};

} // namespace robotick
