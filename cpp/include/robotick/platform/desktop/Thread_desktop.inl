// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/NoStl.h"

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

	inline void Thread::hybrid_sleep_until(Clock::time_point target_time)
	{
		using namespace std_approved::chrono_literals;
		constexpr auto coarse_margin = 2ms;
		constexpr auto coarse_step = 500us;
		constexpr int fine_spin_iters = 20;

		auto now = Clock::now();
		while (now + coarse_margin < target_time)
		{
			std_approved::this_thread::sleep_for(coarse_step);
			now = Clock::now();
		}

		while (Clock::now() < target_time)
		{
			for (volatile int i = 0; i < fine_spin_iters; ++i)
			{
			}
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
		return reinterpret_cast<ThreadId>(pthread_self());
#else
		return 0;
#endif
	}

} // namespace robotick
