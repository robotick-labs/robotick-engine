// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/Thread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std::chrono;

namespace robotick
{

	struct SyncedGroupWorkloadImpl
	{
		struct ChildWorkloadInfo
		{
			std::thread thread;
			std::shared_ptr<std::atomic<uint32_t>> tick_counter = std::make_shared<std::atomic<uint32_t>>(0);
			const WorkloadInstanceInfo* workload = nullptr;
		};

		std::vector<ChildWorkloadInfo> children;

		std::condition_variable tick_cv;
		std::mutex tick_mutex;

		bool running = false;
		double tick_interval_sec = 0.0;

		// Called once by engine before tick loop begins
		void set_children(const std::vector<const WorkloadInstanceInfo*>& child_workloads, std::vector<DataConnectionInfo*>& pending_connections)
		{
			// map from workload pointer to its ChildWorkloadInfo (for fast lookup)
			children.reserve(child_workloads.size());
			std::unordered_map<const WorkloadInstanceInfo*, ChildWorkloadInfo*> workload_to_child;

			// add child workloads and call set_children_fn on each, if present:
			for (const WorkloadInstanceInfo* child_workload : child_workloads)
			{
				ChildWorkloadInfo& info = children.emplace_back();
				info.workload = child_workload;
				workload_to_child[child_workload] = &info;

				if (info.workload && info.workload->type && info.workload->type->set_children_fn)
				{
					info.workload->type->set_children_fn(info.workload->ptr, info.workload->children, pending_connections);
				}
			}

			// classify relevant pending-connections
			for (DataConnectionInfo* conn : pending_connections)
			{
				const bool dst_is_local = workload_to_child.count(conn->dest_workload);
				if (dst_is_local)
				{
					conn->expected_handler = DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine;
				}
			}
		}

		void start(double tick_rate_hz)
		{
			tick_interval_sec = 1.0 / tick_rate_hz;
			running = true;

			for (auto& child : children)
			{
				if (child.workload == nullptr || child.workload->type == nullptr || child.workload->type->tick_fn == nullptr ||
					child.workload->tick_rate_hz == 0.0)
				{
					continue; // don't spawn threads for children that can't / dont need to need
				}

				ChildWorkloadInfo* child_ptr = &child;
				child.thread = std::thread(
					[this, child_ptr]()
					{
						child_tick_loop(*child_ptr);
					});
			}
		}

		void tick(double time_delta)
		{
			tick_interval_sec = time_delta; // update latest interval

			// Notify all running child threads: time to tick (if they're ready)
			// (increment tick-counter too - as their signal)

			for (auto& child : children)
			{
				if (child.thread.joinable()) // only signal running children
				{
					child.tick_counter->fetch_add(1);
				}
			}

			std::lock_guard<std::mutex> lock(tick_mutex);
			tick_cv.notify_all();
		}

		void stop()
		{
			running = false;
			tick_cv.notify_all();
			for (auto& child : children)
			{
				if (child.thread.joinable())
				{
					child.thread.join();
				}
			}
		}

		// Runs on its own thread for each child
		void child_tick_loop(ChildWorkloadInfo& child_info)
		{
			assert(child_info.workload != nullptr); // calling code should have verified this
			const auto& child = *child_info.workload;

			assert(child.type != nullptr && child.type->tick_fn != nullptr && child.tick_rate_hz > 0.0); // calling code should have verified these

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
							return child_info.tick_counter->load() > last_tick || !running;
						});

					last_tick = child_info.tick_counter->load();
				}

				if (!running)
					return;

				// Calculate time since last tick for this child (will be larger than tick_interval_sec if we've missed some ticks)
				auto now = steady_clock::now();
				double time_delta = duration<double>(now - last_tick_time).count();
				last_tick_time = now;

				std::atomic_thread_fence(std::memory_order_acquire);
				// ^- Ensures we see all writes from parent before using shared data

				// Tick the workload with real elapsed time
				child.type->tick_fn(child.ptr, time_delta);
				next_tick_time += tick_interval;

				child.last_time_delta = time_delta;

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

		void set_children(const std::vector<const WorkloadInstanceInfo*>& children, std::vector<DataConnectionInfo*>& pending_connections)
		{
			impl->set_children(children, pending_connections);
		}
		void start(double tick_rate_hz) { impl->start(tick_rate_hz); }
		void tick(double time_delta) { impl->tick(time_delta); }
		void stop() { impl->stop(); }
	};

	static WorkloadAutoRegister<SyncedGroupWorkload> s_auto_register;

} // namespace robotick
