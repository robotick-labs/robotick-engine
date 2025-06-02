// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/platform/Threading.h"
#include <cstdint>

namespace robotick
{
	class ThreadSyncGroup
	{
	  public:
		ThreadSyncGroup();
		~ThreadSyncGroup();

		void notify_all();
		bool wait_for_tick(uint32_t& last_tick, uint32_t current_tick);
		void run_loop(const std::function<void()>& on_tick, const AtomicFlag& exit_flag);

	  private:
		// Forward declare opaque platform-specific members
		struct Impl;
		Impl* impl;
	};
} // namespace robotick

#if defined(ROBOTICK_PLATFORM_ESP32)
#include "robotick/platform/detail/ThreadSyncGroup.esp32.inl"
#elif defined(ROBOTICK_PLATFORM_UBUNTU)
#include "robotick/platform/detail/ThreadSyncGroup.desktop.inl"
#else
#error "Unsupported platform for ThreadSyncGroup"
#endif
