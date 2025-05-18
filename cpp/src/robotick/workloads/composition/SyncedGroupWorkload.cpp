#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/Thread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace robotick
{

	struct SyncedGroupWorkloadImpl
	{
		std::vector<const WorkloadInstanceInfo*> children;
		std::vector<std::thread> child_threads;
		std::vector<std::atomic_flag> child_busy_flags;
		std::vector<std::atomic<uint32_t>> missed_ticks_all_time;

		std::condition_variable tick_cv;
		std::mutex tick_mutex;
		std::vector<std::atomic<uint32_t>> tick_counters;

		bool running = false;
		double tick_interval_sec = 0.0;

		// Called once by engine before tick loop begins
		void set_children(const std::vector<const WorkloadInstanceInfo*>& child_workloads)
		{
			children = child_workloads;

			child_busy_flags = std::vector<std::atomic_flag>(children.size());
			for (auto& flag : child_busy_flags)
			{
				flag.clear();
			}

			missed_ticks_all_time = std::vector<std::atomic<uint32_t>>(children.size());
			tick_counters = std::vector<std::atomic<uint32_t>>(children.size());
			for (auto& tick_counter : tick_counters)
			{
				tick_counter = 0;
			}
		}

		void start(double tick_rate_hz)
		{
			tick_interval_sec = 1.0 / tick_rate_hz;
			running = true;

			for (size_t i = 0; i < children.size(); ++i)
			{
				child_threads.emplace_back(
					[this, i]()
					{
						child_tick_loop(i);
					});
			}
		}

		void tick(double time_delta)
		{
			tick_interval_sec = time_delta; // update latest interval

			// Notify all child threads: time to tick (if they're ready)
			// (increment tick-counter too - as their signal)
			for (auto& tick_counter : tick_counters)
			{
				++tick_counter;
			}

			std::lock_guard<std::mutex> lock(tick_mutex);
			tick_cv.notify_all();
		}

		void stop()
		{
			running = false;
			tick_cv.notify_all();
			for (auto& t : child_threads)
			{
				if (t.joinable())
					t.join();
			}
		}

		// Runs on its own thread for each child
		void child_tick_loop(size_t i)
		{
			assert(children.size() == tick_counters.size());

			auto* child = children[i];
			assert(child);

			auto& child_busy_flag = child_busy_flags[i];

			uint32_t last_tick = 0;
			auto last_tick_time = std::chrono::steady_clock::now();

			set_thread_affinity(2);
			set_thread_priority_high();
			set_thread_name("robotick_syncedgroup_" + std::string(child->type->name) + "_" + child->unique_name);

			while (true)
			{
				// Wait for tick or shutdown signal
				std::unique_lock<std::mutex> lock(tick_mutex);
				tick_cv.wait(lock,
					[&]()
					{
						return tick_counters[i] > last_tick || !running;
					});

				if (!running)
					return;

				last_tick = tick_counters[i];

				lock.unlock();

				// If previous tick still running, skip this one
				if (child_busy_flag.test_and_set(std::memory_order_acquire))
				{
					log_overrun(i);
					++missed_ticks_all_time[i];
					continue;
				}

				// Calculate time since last tick for this child (will be larger than tick_interval_sec if we've missed some ticks)
				auto now = std::chrono::steady_clock::now();
				double time_delta = std::chrono::duration<double>(now - last_tick_time).count();
				last_tick_time = now;

				// Tick the workload with real elapsed time
				if (child != nullptr && child->type->tick_fn != nullptr)
				{
					child->type->tick_fn(child->ptr, time_delta);
				}

				child_busy_flag.clear(std::memory_order_release);
			}
		}

		void log_overrun(size_t i)
		{
			std::printf("[Synced] Overrun: child %zu still running, tick skipped (missed: %u)\n", i, missed_ticks_all_time[i].load());
		}
	};

	struct SyncedGroupWorkload
	{
		SyncedGroupWorkloadImpl* impl = nullptr;

		SyncedGroupWorkload() : impl(new SyncedGroupWorkloadImpl()) {}
		~SyncedGroupWorkload()
		{
			stop();
			delete impl;
		}

		void set_children(const std::vector<const WorkloadInstanceInfo*>& children) { impl->set_children(children); }
		void start(double tick_rate_hz) { impl->start(tick_rate_hz); }
		void tick(double time_delta) { impl->tick(time_delta); }
		void stop() { impl->stop(); }
	};

	static WorkloadAutoRegister<SyncedGroupWorkload> s_auto_register;

} // namespace robotick
