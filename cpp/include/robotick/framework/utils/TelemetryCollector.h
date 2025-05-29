// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Engine.h"
#include "robotick/framework/utils/ConsoleTelemetryTable.h"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

namespace robotick
{
	struct TelemetryCollector
	{
		const Engine& engine;

		std::vector<ConsoleTelemetryRow> collect_rows()
		{
			std::vector<ConsoleTelemetryRow> rows;
			rows.reserve(engine.get_all_instance_info().size());

			const WorkloadInstanceInfo* root = engine.get_root_instance_info();
			assert(root);

			visit_all(root, 0, rows);
			return rows;
		}

	  private:
		void visit_all(const WorkloadInstanceInfo* info, size_t depth, std::vector<ConsoleTelemetryRow>& rows)
		{
			auto& row = rows.emplace_back();
			populate_row(row, depth, *info);
			for (auto* child : info->children)
				visit_all(child, depth + 1, rows);
		}

		void populate_row(ConsoleTelemetryRow& row, size_t depth, const WorkloadInstanceInfo& info)
		{
			row.type = depth_prefix(depth, info.type->name);
			row.name = info.unique_name;
			row.inputs = "?";
			row.outputs = "?";
			row.tick_duration_ms = info.mutable_stats.last_tick_duration * 1000.0;
			row.tick_delta_ms = info.mutable_stats.last_time_delta * 1000.0;
			row.goal_interval_ms = info.tick_rate_hz > 0.0 ? 1000.0 / info.tick_rate_hz : -1.0;
		}

		static std::string depth_prefix(size_t depth, const std::string& name)
		{
			if (depth == 0)
				return name;
			std::ostringstream oss;
			oss << "|";
			for (size_t i = 1; i < depth; ++i)
				oss << "  ";
			oss << "--" << name;
			return oss.str();
		}
	};

} // namespace robotick
