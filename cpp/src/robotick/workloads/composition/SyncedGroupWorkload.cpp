// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/platform/Threading.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace robotick
{

	struct SyncedGroupWorkloadImpl
	{
		struct ChildWorkloadInfo
		{
			Thread thread;
			std::shared_ptr<std::atomic<uint32_t>> tick_counter = std::make_shared<std::atomic<uint32_t>>(0);
			const WorkloadInstanceInfo* workload_info = nullptr;
			void* workload_ptr = nullptr;
		};

		const Engine* engine = nullptr;
		std::vector<ChildWorkloadInfo> children;

		std::condition_variable tick_cv;
		std::mutex tick_mutex;

		bool running = false;

		void set_engine(const Engine& engine_in) { engine = &engine_in; }

		void set_children(const std::vector<const WorkloadInstanceInfo*>& child_workloads, std::vector<DataConnectionInfo*>& pending_connections)
		{
			ROBOTICK_ASSERT(engine != nullptr);

			children.reserve(child_workloads.size());
			std::unordered_map<const WorkloadInstanceInfo*, ChildWorkloadInfo*> workload_to_child;

			for (const WorkloadInstanceInfo* child_workload : child_workloads)
			{
				ChildWorkloadInfo& info = children.emplace_back();
				info.workload_info = child_workload;
				info.workload_ptr = child_workload->get_ptr(*engine);
				workload_to_child[child_workload] = &info;

				if (auto fn = child_workload->type ? child_workload->type->set_children_fn : nullptr)
				{
					fn(info.workload_ptr, child_workload->children, pending_connections);
				}
			}

			for (DataConnectionInfo* conn : pending_connections)
			{
				if (workload_to_child.count(conn->dest_workload))
				{
					conn->expected_handler = DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine;
				}
			}
		}

		void start(double)
		{
			running = true;

			for (auto& child : children)
			{
				if (!child.workload_info || !child.workload_info->type || !child.workload_info->type->tick_fn ||
					child.workload_info->tick_rate_hz == 0.0)
				{
					continue;
				}

				struct ThreadContext
				{
					SyncedGroupWorkloadImpl* impl;
					ChildWorkloadInfo* child;
				};

				ThreadContext* ctx = new ThreadContext{this, &child};

				child.thread = Thread(
					[](void* raw)
					{
						auto* ctx = static_cast<ThreadContext*>(raw);
						ctx->impl->child_tick_loop(*ctx->child);
						delete ctx;
					},
					ctx, child.workload_info->unique_name.substr(0, 15), 2);
			}
		}

		void tick(const TickInfo&)
		{
			// note - we don't use the supplied TickInfo as we don't need if for ourselves, and our children are allowed to tick at their requested
			// rate (as long as equal to or slower than our tick rate).  That is enforced in Model validation code.

			for (auto& child : children)
			{
				child.tick_counter->fetch_add(1);
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
				if (child.thread.is_joining_supported() && child.thread.is_joinable())
				{
					child.thread.join();
				}
			}
		}

		void child_tick_loop(ChildWorkloadInfo& child_info)
		{
			ROBOTICK_ASSERT(child_info.workload_info);
			const auto& child = *child_info.workload_info;

			ROBOTICK_ASSERT(child.type && child.type->tick_fn && child.tick_rate_hz > 0.0);

			uint32_t last_tick = 0;
			const auto child_start_time = std::chrono::steady_clock::now();
			auto last_tick_time = child_start_time;
			auto next_tick_time = child_start_time;

			const auto tick_interval_sec = std::chrono::duration<double>(1.0 / child.tick_rate_hz);
			const auto tick_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_interval_sec);

			Thread::set_name(child.unique_name.substr(0, 15));
			Thread::set_affinity(2);
			Thread::set_priority_high();

			TickInfo tick_info;

			while (true)
			{
				{
					std::unique_lock<std::mutex> lock(tick_mutex);
					tick_cv.wait(lock,
						[&]
						{
							return child_info.tick_counter->load() > last_tick || !running;
						});
					last_tick = child_info.tick_counter->load();
				}

				if (!running)
					return;

				const auto now = std::chrono::steady_clock::now();
				const auto ns_since_start = std::chrono::duration_cast<std::chrono::nanoseconds>(now - child_start_time).count();
				const auto ns_since_last = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_tick_time).count();

				tick_info.tick_count += 1;
				tick_info.time_now_ns = ns_since_start;
				tick_info.time_now = ns_since_start * 1e-9;
				tick_info.delta_time = ns_since_last * 1e-9;

				last_tick_time = now;

				std::atomic_thread_fence(std::memory_order_acquire);

				child.type->tick_fn(child_info.workload_ptr, tick_info);
				next_tick_time += tick_interval;

				const auto now_post = std::chrono::steady_clock::now();
				child.mutable_stats.last_tick_duration = std::chrono::duration<double>(now_post - now).count();
				child.mutable_stats.last_time_delta = tick_info.delta_time;

				Thread::hybrid_sleep_until(std::chrono::time_point_cast<std::chrono::steady_clock::duration>(next_tick_time));
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

		void set_engine(const Engine& engine_in) { impl->set_engine(engine_in); }
		void set_children(const std::vector<const WorkloadInstanceInfo*>& children, std::vector<DataConnectionInfo*>& pending_connections)
		{
			impl->set_children(children, pending_connections);
		}
		void start(double tick_rate_hz) { impl->start(tick_rate_hz); }
		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }
		void stop() { impl->stop(); }
	};

	ROBOTICK_DEFINE_WORKLOAD(SyncedGroupWorkload)

} // namespace robotick
