// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

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
			const WorkloadInstanceInfo* workload = nullptr;
			std::vector<const DataConnectionInfo*> connections_in;
		};

		std::vector<ChildWorkloadInfo> children;

		void set_children(const std::vector<const WorkloadInstanceInfo*>& child_workloads, std::vector<DataConnectionInfo*>& pending_connections)
		{
			// map from workload pointer to its ChildWorkloadInfo (for fast lookup)
			children.reserve(child_workloads.size()); // <- reserve so we don't keep reallocating children during population
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

			// iterate + classify connections
			for (auto it = pending_connections.begin(); it != pending_connections.end();)
			{
				DataConnectionInfo* pending_connection = *it;
				assert(pending_connection != nullptr); // we should never be passed null connections

				const bool src_is_local = workload_to_child.count(pending_connection->source_workload);
				const bool dst_is_local = workload_to_child.count(pending_connection->dest_workload);

				if (src_is_local && dst_is_local)
				{
					// Internal connection
					const auto child_workload_info = workload_to_child[pending_connection->dest_workload];
					child_workload_info->connections_in.push_back(pending_connection);

					assert(pending_connection->expected_handler == DataConnectionInfo::ExpectedHandler::Unassigned);
					pending_connection->expected_handler = DataConnectionInfo::ExpectedHandler::SequencedGroupWorkload;

					it = pending_connections.erase(it);
				}
				else if (dst_is_local)
				{
					// Source external, we're the receiver
					pending_connection->expected_handler = DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine;
					++it;
				}
				else
				{
					++it; // Not ours to handle
				}
			}
		}

		void start(double) { /* nothing needed */ }

		void tick(double time_delta)
		{
			auto start_time = std::chrono::steady_clock::now();

			for (auto& child_info : children)
			{
				if (child_info.workload != nullptr && child_info.workload->type->tick_fn != nullptr)
				{
					// process any incoming data-connections:
					for (auto connection_in : child_info.connections_in)
					{
						connection_in->do_data_copy();
					}

					// tick the child:
					child_info.workload->type->tick_fn(child_info.workload->ptr, time_delta);
				}
			}

			auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start_time).count();

			if (elapsed > time_delta)
			{
				std::printf("[Sequenced] Overrun: tick took %.3fms (budget %.3fms)\n", elapsed * 1000.0, time_delta * 1000.0);
			}
		}

		void stop() {} // nothing to do
	};

	struct SequencedGroupWorkload
	{
		SequencedGroupWorkloadImpl* impl = nullptr;

		SequencedGroupWorkload() : impl(new SequencedGroupWorkloadImpl()) {}
		~SequencedGroupWorkload()
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

	static WorkloadAutoRegister<SequencedGroupWorkload> s_auto_register;

} // namespace robotick
