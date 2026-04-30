// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/memory/StdApproved.h"
#include "robotick/framework/strings/FixedString.h"

#include <atomic>
#include <chrono>
#include <cwchar>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace robotick
{

	inline Thread::Thread(EntryPoint fn, void* arg, const char* name, int core, int /*stack_size*/, int /*priority*/)
		: thread(
			  [fn, arg, safe_name = FixedString64(name ? name : ""), core]()
			  {
				  Thread::set_name(safe_name.c_str());
				  if (core >= 0)
				  {
					  Thread::set_affinity(core);
				  }
				  Thread::set_priority_high();
				  fn(arg);
			  })
	{
	}

	inline Thread::~Thread()
	{
		if (thread.joinable())
			thread.join();
	}

	inline bool Thread::is_joining_supported() const
	{
		return true; // this platform supports joining threads
	}

	inline bool Thread::is_joinable() const
	{
		return thread.joinable();
	}

	inline void Thread::join()
	{
		if (thread.joinable())
			thread.join();
	}

	inline void Thread::sleep_ms(uint32_t ms)
	{
		std_approved::this_thread::sleep_for(std_approved::chrono::milliseconds(ms));
	}

	inline void Thread::yield()
	{
		std_approved::this_thread::yield();
	}

	inline uint64_t to_non_negative_ns(Clock::duration value)
	{
		const int64_t ns = Clock::to_nanoseconds(value).count();
		return (ns > 0) ? static_cast<uint64_t>(ns) : 0;
	}

	inline void Thread::hybrid_sleep_until(Clock::time_point target_time, HybridSleepMode mode, HybridSleepStats* out_stats)
	{
		const Clock::time_point now = Clock::now();

		if (out_stats != nullptr)
		{
			*out_stats = HybridSleepStats{};
			out_stats->mode = mode;
			out_stats->requested_wait_ns = to_non_negative_ns(target_time - now);
		}

		if (target_time <= now)
			return;

		if (mode == HybridSleepMode::Eco)
		{
			// cheap and direct, but scheduler/timer wake latency / granularity can still be hundreds of microseconds or worse.
			const Clock::time_point coarse_phase_start = (out_stats != nullptr) ? Clock::now() : now;

			std_approved::this_thread::sleep_until(target_time);

			if (out_stats != nullptr)
			{
				const Clock::time_point coarse_phase_end = Clock::now();
				out_stats->coarse_sleep_ns += to_non_negative_ns(coarse_phase_end - coarse_phase_start);
				out_stats->total_wait_ns = to_non_negative_ns(coarse_phase_end - now);
				out_stats->wake_lateness_ns = to_non_negative_ns(coarse_phase_end - target_time);
			}
			return;
		}

		constexpr std_approved::chrono::milliseconds coarse_sleep_margin(2);
		constexpr std_approved::chrono::microseconds spin_margin(50);
		constexpr int fine_spin_iters = 20;

		// Stage 1 (coarse sleep): hand off most of the wait budget to the OS.
		if (target_time - now > coarse_sleep_margin)
		{
			const Clock::time_point sleep_deadline = target_time - coarse_sleep_margin;
			const Clock::time_point current_time = Clock::now();
			if (sleep_deadline > current_time)
			{
				const Clock::time_point coarse_phase_start = (out_stats != nullptr) ? current_time : now;
				std_approved::this_thread::sleep_for(sleep_deadline - current_time);
				if (out_stats != nullptr)
				{
					const Clock::time_point coarse_phase_end = Clock::now();
					out_stats->coarse_sleep_ns += to_non_negative_ns(coarse_phase_end - coarse_phase_start);
				}
			}
		}

		Clock::time_point yield_deadline = target_time;
		if (mode == HybridSleepMode::Accurate)
		{
			yield_deadline = target_time - spin_margin;
		}

		// Stage 2 (yield): Normal yields to the deadline; Accurate yields until the final spin slice.
		const Clock::time_point yield_phase_start = (out_stats != nullptr) ? Clock::now() : now;
		while (Clock::now() < yield_deadline)
		{
			if (out_stats != nullptr)
			{
				out_stats->yield_iterations++;
			}
			std_approved::this_thread::yield();
		}
		if (out_stats != nullptr)
		{
			const Clock::time_point yield_phase_end = Clock::now();
			out_stats->yield_phase_ns += to_non_negative_ns(yield_phase_end - yield_phase_start);
		}

		if (mode == HybridSleepMode::Accurate)
		{
			// Stage 3 (spin, Accurate only): hold the final micro-window for tighter wake control.
			// Accurate spends only the last small slice in a tight loop to reduce wake lateness.
			// This gives finer deadline control than yield(): a yield asks the scheduler to run
			// something else and can resume unpredictably later, while this loop keeps ownership
			// of the final micro-window and exits as soon as target_time is reached.
			const Clock::time_point spin_phase_start = (out_stats != nullptr) ? Clock::now() : now;
			while (Clock::now() < target_time)
			{
				if (out_stats != nullptr)
				{
					out_stats->spin_iterations++;
				}
				for (volatile int i = 0; i < fine_spin_iters; ++i)
				{
				}
			}
			if (out_stats != nullptr)
			{
				const Clock::time_point spin_phase_end = Clock::now();
				out_stats->spin_phase_ns += to_non_negative_ns(spin_phase_end - spin_phase_start);
			}
		}

		if (out_stats != nullptr)
		{
			const Clock::time_point finish_time = Clock::now();
			out_stats->total_wait_ns = to_non_negative_ns(finish_time - now);
			out_stats->wake_lateness_ns = to_non_negative_ns(finish_time - target_time);
		}
	}

	inline void Thread::set_name(const char* name)
	{
#if defined(_WIN32)
		wchar_t buffer[64];
		if (!name)
			name = "";
		int written = MultiByteToWideChar(CP_UTF8, 0, name, -1, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
		if (written <= 0)
		{
			buffer[0] = L'\0';
		}
		SetThreadDescription(GetCurrentThread(), buffer);
#elif defined(__linux__)
		pthread_setname_np(pthread_self(), name ? name : "");
#endif
	}

	inline void Thread::set_priority_high()
	{
#if defined(_WIN32)
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#elif defined(__linux__)
		sched_param sch_params;
		sch_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
		pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_params);
#endif
	}

	inline void Thread::set_affinity(int core)
	{
#if defined(_WIN32)
		if (core < 0 || core >= static_cast<int>(sizeof(DWORD_PTR) * 8))
		{
			ROBOTICK_FATAL_EXIT("Thread::set_affinity: Invalid core index %d (system supports up to %llu)", core, sizeof(DWORD_PTR) * 8);
		}
		DWORD_PTR mask = static_cast<DWORD_PTR>(1) << core;
		SetThreadAffinityMask(GetCurrentThread(), mask);
#elif defined(__linux__)
		if (core < 0 || core >= CPU_SETSIZE)
		{
			ROBOTICK_FATAL_EXIT("Thread::set_affinity: Invalid core index %d (must be in [0, %d])", core, CPU_SETSIZE - 1);
		}

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(core, &cpuset);

		pthread_t current_thread = pthread_self();
		if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0)
		{
			ROBOTICK_FATAL_EXIT("Thread::set_affinity: pthread_setaffinity_np failed");
		}
#endif
	}

	inline Thread::ThreadId Thread::get_current_thread_id()
	{
#if defined(_WIN32)
		return static_cast<ThreadId>(GetCurrentThreadId());
#elif defined(__linux__)
		return static_cast<ThreadId>(pthread_self());
#else
		return 0;
#endif
	}

	inline uint32_t Thread::get_hardware_concurrency()
	{
		return std_approved::thread::hardware_concurrency();
	}

} // namespace robotick
