// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

// This header is the only place the engine core pulls in C++ standard headers directly.
// Everything exposed here is safe for deterministic, heap-free MCU usage (chrono traits, type traits,
// atomics, sorting helpers, small utilities) and is wrapped inside robotick::std_approved.
// If you need anything else from the STL you must expand this list intentionally so we can
// reason about heap usage and platform behaviour in one place.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

namespace robotick
{
	namespace std_approved = std;
} // namespace robotick
