// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace robotick
{

	inline Thread::Thread(EntryPoint fn, void* arg, const std::string& name, int core, int /*stack_size*/, int /*priority*/)
		: thread(
			  [fn, arg, name, core]()
			  {
				  Thread::set_name(name);
				  Thread::set_affinity(core);
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

	inline void Thread::join()
	{
		if (thread.joinable())
			thread.join();
	}

	inline bool Thread::is_joinable() const
	{
		return thread.joinable();
	}

	inline void Thread::sleep_ms(uint32_t ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	inline void Thread::set_name(const std::string& name)
	{
#if defined(_WIN32)
		std::wstring wname(name.begin(), name.end());
		SetThreadDescription(GetCurrentThread(), wname.c_str());
#elif defined(__APPLE__)
		pthread_setname_np(name.c_str());
#elif defined(__linux__)
		pthread_setname_np(pthread_self(), name.c_str());
#endif
	}

	inline void Thread::set_priority_high()
	{
#if defined(_WIN32)
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
	}

	inline void Thread::set_affinity(int core)
	{
#if defined(_WIN32)
		DWORD_PTR mask = 1 << core;
		SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
	}

	inline void Thread::yield()
	{
		std::this_thread::yield();
	}

	inline void Thread::hybrid_sleep_until(std::chrono::steady_clock::time_point target_time)
	{
		using namespace std::chrono_literals;
		constexpr auto coarse_margin = 2ms;
		constexpr auto coarse_step = 500us;
		constexpr int fine_spin_iters = 20;

		auto now = std::chrono::steady_clock::now();
		while (now + coarse_margin < target_time)
		{
			std::this_thread::sleep_for(coarse_step);
			now = std::chrono::steady_clock::now();
		}

		while (std::chrono::steady_clock::now() < target_time)
		{
			for (volatile int i = 0; i < fine_spin_iters; ++i)
			{
			}
		}
	}

} // namespace robotick
