// Copyright 2025 Robotick Labs CIC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


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

using namespace std::chrono;

namespace robotick
{

	struct SyncedGroupWorkloadImpl
	{
		std::vector<const WorkloadInstanceInfo*> children;
		std::vector<std::thread> child_threads;

		std::condition_variable tick_cv;
		std::mutex tick_mutex;
		std::vector<std::atomic<uint32_t>> tick_counters;

		bool running = false;
		double tick_interval_sec = 0.0;

		// Called once by engine before tick loop begins
		void set_children(const std::vector<const WorkloadInstanceInfo*>& child_workloads)
		{
			children = child_workloads;

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
				const auto* child = children[i];

				if (child == nullptr || child->type == nullptr || child->type->tick_fn == nullptr || child->tick_rate_hz == 0.0)
				{
					continue; // don't spawn threads for children that can't / dont need to need
				}

				child_threads.emplace_back(
					[this, i, child]()
					{
						child_tick_loop(i, *child);
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
		void child_tick_loop(size_t child_index, const WorkloadInstanceInfo& child)
		{
			assert(children.size() == tick_counters.size());
			assert(child.type != nullptr && child.type->tick_fn != nullptr && child.tick_rate_hz > 0.0);

			uint32_t last_tick = 0;
			auto next_tick_time = steady_clock::now();
			auto last_tick_time = steady_clock::now();

			const auto tick_interval_sec = duration<double>(1.0 / child.tick_rate_hz);
			const auto tick_interval = duration_cast<steady_clock::duration>(tick_interval_sec);

			set_thread_affinity(2);
			set_thread_priority_high();

			std::string thread_name = child.unique_name;
			thread_name = thread_name.substr(0, 15); // linux doesn't like thread-names more than 16 characters incl /0
			set_thread_name(thread_name);

			while (true)
			{
				// Wait for tick or shutdown signal (lock scoped with braces):
				{
					std::unique_lock<std::mutex> lock(tick_mutex);
					tick_cv.wait(lock,
						[&]()
						{
							return tick_counters[child_index] > last_tick || !running;
						});

					last_tick = tick_counters[child_index];
				}

				if (!running)
					return;

				// Calculate time since last tick for this child (will be larger than tick_interval_sec if we've missed some ticks)
				auto now = steady_clock::now();
				double time_delta = duration<double>(now - last_tick_time).count();
				last_tick_time = now;

				// Tick the workload with real elapsed time
				child.type->tick_fn(child.ptr, time_delta);
				next_tick_time += tick_interval;

				// ensure that we honour the desired tick-rate of every child (even if some slower than this SyncedGroup's tick-rate - just let them
				// fall back into step when ready):
				hybrid_sleep_until(time_point_cast<steady_clock::duration>(next_tick_time));
			}
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
