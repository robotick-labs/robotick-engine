// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <new>
#include <type_traits>

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
			"State<T>: Type too large for inline storage.\n"
			"Use StatePtr<T> instead.\n"
			"sizeof(T) = " __FILE__); // dummy to ensure compile fail; message below improves

		// Trick: produce more helpful size info using a constexpr check
		static constexpr bool size_okay = sizeof(T) <= MaxReasonableInlineStateSize || (throw "State<T>: Type too large for inline storage. "
																							  "sizeof(T) exceeds 5KB threshold.",
																						   false);

	  public:
		State() { new (&storage) T(); }
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
		StatePtr() { ptr = new T(); }

		~StatePtr()
		{
			delete ptr;
			ptr = nullptr;
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
