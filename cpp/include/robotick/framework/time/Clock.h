// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <chrono>
#include <cstdint>

namespace robotick
{
	namespace detail
	{
		inline uint32_t clamp_to_uint32(uint64_t value)
		{
			return (value > UINT32_MAX) ? UINT32_MAX : static_cast<uint32_t>(value);
		}
	} // namespace detail

	struct Clock
	{
		using SteadyClock = std_approved::chrono::steady_clock;
		using time_point = SteadyClock::time_point;
		using duration = SteadyClock::duration;
		using nanoseconds = std_approved::chrono::nanoseconds;

		static time_point now() noexcept { return SteadyClock::now(); }

		template <typename Floating> static duration from_seconds(Floating seconds)
		{
			return std_approved::chrono::duration_cast<duration>(std_approved::chrono::duration<Floating>(seconds));
		}

		template <typename DurationType> static duration duration_cast(const DurationType& value)
		{
			return std_approved::chrono::duration_cast<duration>(value);
		}

		static nanoseconds to_nanoseconds(duration value) { return std_approved::chrono::duration_cast<nanoseconds>(value); }
	};

} // namespace robotick
