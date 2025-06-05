// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <new>
#include <type_traits>

namespace robotick
{

	//------------------------------------------------------------------------------
	// State<T> - inline-only, destructor-safe state wrapper
	//------------------------------------------------------------------------------

	template <typename T> class State
	{
		static_assert(std::is_default_constructible<T>::value, "State<T> requires T to be default-constructible");

	  public:
		// Constructor: placement-new the state
		State() { new (&storage) T(); }

		// Destructor: manually call T's destructor
		~State() { get().~T(); }

		// Enable arrow access for ergonomic usage
		T* operator->() { return &get(); }
		const T* operator->() const { return &get(); }

		// Implicit conversion to T& for convenience
		operator T&() { return get(); }
		operator const T&() const { return get(); }

		// Explicit access
		T& get() { return *reinterpret_cast<T*>(&storage); }
		const T& get() const { return *reinterpret_cast<const T*>(&storage); }

	  private:
		alignas(T) uint8_t storage[sizeof(T)];
	};

} // namespace robotick
