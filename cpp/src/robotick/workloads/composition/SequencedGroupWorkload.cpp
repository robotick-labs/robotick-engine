// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/DataConnection.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <vector>

namespace robotick
{
	struct SequencedGroupWorkloadImpl
	{
		struct ChildWorkloadInfo
		{
			const WorkloadInstanceInfo* workload_info = nullptr;
			void* workload_ptr = nullptr;

			List<const DataConnectionInfo*> connections_in;
		};

		const Engine* engine = nullptr;
		HeapVector<ChildWorkloadInfo> children;

		void set_engine(const Engine& engine_in) { engine = &engine_in; }

		ChildWorkloadInfo* find_child_workload(const WorkloadInstanceInfo& query_child)
		{
			for (ChildWorkloadInfo& child : children)
			{
				if (child.workload_info == &query_child)
				{
					return &child;
				}
			}

			return nullptr;
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

				ROBOTICK_ASSERT(info.workload_info && info.workload_info->type);
				const WorkloadDescriptor* workload_desc = info.workload_info->type->get_workload_desc();
				ROBOTICK_ASSERT(workload_desc);

				if (workload_desc->set_children_fn)
				{
					workload_desc->set_children_fn(info.workload_ptr, info.workload_info->children, pending_connections);
				}
			}

			// iterate + classify connections
			for (DataConnectionInfo& conn : pending_connections)
			{
				if (conn.expected_handler != DataConnectionInfo::ExpectedHandler::Unassigned)
				{
					continue;
				}

				const bool src_is_local = find_child_workload(*conn.source_workload) != nullptr;

				ChildWorkloadInfo* dest_child_info = find_child_workload(*conn.dest_workload);
				const bool dst_is_local = (dest_child_info != nullptr);

				if (src_is_local && dst_is_local)
				{
					ROBOTICK_ASSERT(dest_child_info != nullptr);
					dest_child_info->connections_in.push_back(&conn);
					conn.expected_handler = DataConnectionInfo::ExpectedHandler::SequencedGroupWorkload;
				}
				else
				{
					if (dst_is_local)
					{
						conn.expected_handler = DataConnectionInfo::ExpectedHandler::DelegateToParent;
					}
				}
			}
		}

		void tick(const TickInfo& tick_info)
		{
			ROBOTICK_ASSERT(engine != nullptr && "Engine should have been set by now");

			auto start_time = std::chrono::steady_clock::now();

			for (auto& child_info : children)
			{
				if (child_info.workload_info != nullptr && child_info.workload_info->workload_descriptor->tick_fn != nullptr)
				{
					const auto now_pre_tick = std::chrono::steady_clock::now();

					// process any incoming data-connections:
					for (auto connection_in : child_info.connections_in)
					{
						connection_in->do_data_copy();
					}

					// tick the child:
					child_info.workload_info->workload_descriptor->tick_fn(child_info.workload_ptr, tick_info);

					const auto now_post_tick = std::chrono::steady_clock::now();
					child_info.workload_info->mutable_stats.last_tick_duration_ns =
						static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now_post_tick - now_pre_tick).count());

					constexpr float NanosecondsPerSecond = 1e9f;
					child_info.workload_info->mutable_stats.last_time_delta_ns = static_cast<uint32_t>(tick_info.delta_time * NanosecondsPerSecond);
				}
			}

			auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start_time).count();

			if (elapsed > tick_info.delta_time)
			{
				std::printf("[Sequenced] Overrun: tick took %.3fms (budget %.3fms)\n", elapsed * 1000.0, tick_info.delta_time * 1000.0);
			}
		}
	};

	struct SequencedGroupWorkload
	{
		SequencedGroupWorkloadImpl* impl = nullptr;

		SequencedGroupWorkload()
			: impl(new SequencedGroupWorkloadImpl())
		{
		}
		~SequencedGroupWorkload()
		{
			stop();
			delete impl;
		}

		void set_engine(const Engine& engine_in) { impl->set_engine(engine_in); }

		void set_children(const HeapVector<const WorkloadInstanceInfo*>& children, HeapVector<DataConnectionInfo>& pending_connections)
		{
			impl->set_children(children, pending_connections);
		}

		void start(double) { /* placeholder for consistency with SequencedGroup*/ }

		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }

		void stop() { /* placeholder for consistency with SequencedGroup*/ }
	};

	ROBOTICK_REGISTER_WORKLOAD(SequencedGroupWorkload)

} // namespace robotick
