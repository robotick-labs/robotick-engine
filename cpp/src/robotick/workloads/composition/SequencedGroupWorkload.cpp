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

			std::vector<const DataConnectionInfo*> connections_in;
		};

		const Engine* engine = nullptr;
		std::vector<ChildWorkloadInfo> children;

		void set_engine(const Engine& engine_in) { engine = &engine_in; }

		void set_children(const HeapVector<const WorkloadInstanceInfo*>& child_workloads, const HeapVector<DataConnectionInfo>& pending_connections)
		{
			ROBOTICK_ASSERT(engine != nullptr && "Engine should have been set by now");

			// map from workload_info pointer to its ChildWorkloadInfo (for fast lookup)
			children.reserve(child_workloads.size()); // <- reserve so we don't keep reallocating children during population
			std::unordered_map<const WorkloadInstanceInfo*, ChildWorkloadInfo*> workload_to_child;

			// add child workloads and call set_children_fn on each, if present:
			for (const WorkloadInstanceInfo* child_workload : child_workloads)
			{
				ChildWorkloadInfo& info = children.emplace_back();
				info.workload_info = child_workload;
				info.workload_ptr = child_workload->get_ptr(*engine);

				workload_to_child[child_workload] = &info;

				ROBOTICK_ASSERT(info.workload_info && info.workload_info->type);
				WorkloadDescriptor* workload_desc = info.workload_info->type->get_workload_desc();
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

				const auto src_it = workload_to_child.find(conn.source_workload);
				const auto dst_it = workload_to_child.find(conn.dest_workload);
				const bool src_is_local = src_it != workload_to_child.end();
				const bool dst_is_local = dst_it != workload_to_child.end();

				if (src_is_local && dst_is_local)
				{
					dst_it->second->connections_in.push_back(conn);
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

		void set_children(const HeapVector<const WorkloadInstanceInfo*>& children, const HeapVector<DataConnectionInfo>& pending_connections)
		{
			impl->set_children(children, pending_connections);
		}

		void start(double) { /* placeholder for consistency with SequencedGroup*/ }

		void tick(const TickInfo& tick_info) { impl->tick(tick_info); }

		void stop() { /* placeholder for consistency with SequencedGroup*/ }
	};

	ROBOTICK_REGISTER_WORKLOAD(SequencedGroupWorkload)

} // namespace robotick
