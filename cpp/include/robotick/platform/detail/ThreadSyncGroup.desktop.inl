#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace robotick
{
	struct ThreadSyncGroup::Impl
	{
		std::mutex mutex;
		std::condition_variable cond;
	};

	inline ThreadSyncGroup::ThreadSyncGroup()
	{
		impl = new Impl;
	}

	inline ThreadSyncGroup::~ThreadSyncGroup()
	{
		delete impl;
	}

	inline void ThreadSyncGroup::notify_all()
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		impl->cond.notify_all();
	}

	inline bool ThreadSyncGroup::wait_for_tick(uint32_t& last_tick, uint32_t current_tick)
	{
		std::unique_lock<std::mutex> lock(impl->mutex);
		impl->cond.wait(lock,
			[&]()
			{
				return last_tick != current_tick;
			});
		last_tick = current_tick;
		return true;
	}

	inline void ThreadSyncGroup::run_loop(const std::function<void()>& on_tick, const AtomicFlag& exit_flag)
	{
		uint32_t tick_counter = 0;
		while (!exit_flag.is_set())
		{
			{
				std::lock_guard<std::mutex> lock(impl->mutex);
				on_tick();
				tick_counter++;
			}
			impl->cond.notify_all();								   // wake up any waiting threads
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // optional: simulate 1ms tick
		}
	}

} // namespace robotick
