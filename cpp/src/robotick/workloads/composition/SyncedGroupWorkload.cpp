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
		HeapVector<ChildWorkloadInfo> children;

		std::condition_variable tick_cv;
		std::mutex tick_mutex;

		bool running = false;

		void set_engine(const Engine& engine_in) { engine = &engine_in; }

		ChildWorkloadInfo* find_child_workload(const WorkloadInstanceInfo& query_child)
		{
			for (ChildWorkloadInfo& child : children)
			{
				if (child.workload_info == &query_child)
				{
					return &child;
				}

				return nullptr;
			}
		}

		void set_children(const HeapVector<const WorkloadInstanceInfo*>& child_workloads, HeapVector<DataConnectionInfo>& pending_connections)
		{
			ROBOTICK_ASSERT(engine != nullptr && "Engine should have been set by now");

			children.initialize(child_workloads.size());
			size_t child_index = 0;

			// add child workloads and call set_children_fn on each, if present:
			for (const WorkloadInstanceInfo* child_workload : child_workloads)
			{
				ChildWorkloadInfo& info = children[child_index];
				child_index++;

				info.workload_info = child_workload;
				info.workload_ptr = child_workload->get_ptr(*engine);

				ROBOTICK_ASSERT(child_workload->workload_descriptor != nullptr);

				if (child_workload->workload_descriptor->set_children_fn != nullptr)
				{
					child_workload->workload_descriptor->set_children_fn(info.workload_ptr, child_workload->children, pending_connections);
				}

				for (DataConnectionInfo& conn : pending_connections)
				{
					if (conn.expected_handler != DataConnectionInfo::ExpectedHandler::Unassigned)
					{
						continue;
					}
					else if (conn.dest_workload == info.workload_info)
					{
						conn.expected_handler = DataConnectionInfo::ExpectedHandler::DelegateToParent;
					}
				}
			}
		}

		void start(double)
		{
			running = true;

			for (auto& child : children)
			{
				if (!child.workload_info || !child.workload_info->workload_descriptor || !child.workload_info->workload_descriptor->tick_fn ||
					child.workload_info->seed->tick_rate_hz == 0.0)
				{
					continue;
				}

				struct ThreadContext
				{
					SyncedGroupWorkloadImpl* impl;
					ChildWorkloadInfo* child;
				};

				ThreadContext* ctx = new ThreadContext{this, &child};

				const std::string thread_name(child.workload_info->seed->unique_name.c_str(), 15);

				child.thread = Thread(
					[](void* raw)
					{
						auto* ctx = static_cast<ThreadContext*>(raw);
						ctx->impl->child_tick_loop(*ctx->child);
						delete ctx;
					},
					ctx,
					thread_name,
					2);
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

			ROBOTICK_ASSERT(child.type && child.workload_descriptor->tick_fn && child.seed->tick_rate_hz > 0.0);

			uint32_t last_tick = 0;
			const auto child_start_time = std::chrono::steady_clock::now();
			auto last_tick_time = child_start_time;
			auto next_tick_time = child_start_time;

			const auto tick_interval_sec = std::chrono::duration<double>(1.0 / child.seed->tick_rate_hz);
			const auto tick_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_interval_sec);

			std::string thread_name(child.seed->unique_name.c_str(), 15);

			Thread::set_name(thread_name);
			Thread::set_affinity(2);
			Thread::set_priority_high();

			TickInfo tick_info;

			auto workload_tick_fn = child.workload_descriptor->tick_fn;

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

				workload_tick_fn(child_info.workload_ptr, tick_info);
				next_tick_time += tick_interval;

				const auto now_post = std::chrono::steady_clock::now();
				child.mutable_stats.last_tick_duration_ns =
					static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now_post - now).count());

				constexpr float NanosecondsPerSecond = 1e9f;
				child.mutable_stats.last_time_delta_ns = static_cast<uint32_t>(tick_info.delta_time * NanosecondsPerSecond);

				Thread::hybrid_sleep_until(std::chrono::time_point_cast<std::chrono::steady_clock::duration>(next_tick_time));
			}
		}
	};

	struct SyncedGroupWorkload
	{
		SyncedGroupWorkloadImpl* impl = nullptr;

		SyncedGroupWorkload()
			: impl(new SyncedGroupWorkloadImpl())
		{
		}
		~SyncedGroupWorkload()
		{
			stop();
			delete impl;
		}

		void set_engine(const Engine& engine_in) { impl->set_engine(engine_in); }
		void set_children(const HeapVector<const WorkloadInstanceInfo*>& children, HeapVector<DataConnectionInfo>& pending_connections)
		{
			impl->set_children(children, pending_connections);
		}
		void start(double tick_rate_hz) { impl->start(tick_rate_hz); }
		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }
		void stop() { impl->stop(); }
	};

	ROBOTICK_REGISTER_WORKLOAD(SyncedGroupWorkload)

} // namespace robotick
