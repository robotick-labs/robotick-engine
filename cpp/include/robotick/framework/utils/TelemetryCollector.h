// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/utils/ConsoleTelemetryTable.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

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

		static inline std::string join(const std::vector<std::string>& parts, const std::string& delim)
		{
			if (parts.empty())
				return {};

			std::ostringstream result;
			result << parts[0];
			for (size_t i = 1; i < parts.size(); ++i)
			{
				result << delim << parts[i];
			}
			return result.str();
		}

		void populate_row(ConsoleTelemetryRow& row, size_t depth, const WorkloadInstanceInfo& info)
		{
			row.type = depth_prefix(depth, info.type->name);
			row.name = info.unique_name;

			std::vector<std::string> input_entries;
			std::vector<std::string> output_entries;

			WorkloadFieldsIterator::for_each_field_in_workload(engine, info, nullptr,
				[&](const WorkloadFieldView& view)
				{
					if (view.struct_info == view.instance->type->config_struct)
						return; // skip config

					std::ostringstream entry;
					entry << view.field->name.c_str();

					if (view.subfield)
						entry << "." << view.subfield->name.c_str();

					entry << "=";

					if (view.subfield)
					{
						const std::type_index& type = view.subfield->type;
						if (type == typeid(int))
							entry << *static_cast<const int*>(view.field_ptr);
						else if (type == typeid(double))
							entry << *static_cast<const double*>(view.field_ptr);
						else if (type == typeid(FixedString64))
							entry << "\"" << static_cast<const FixedString64*>(view.field_ptr)->c_str() << "\"";
						else if (type == typeid(FixedString128))
							entry << "\"" << static_cast<const FixedString128*>(view.field_ptr)->c_str() << "\"";
						else
							entry << "<unsupported>";
					}
					else
					{
						// fallback for top-level (non-blackboard) fields
						const std::type_index& type = view.field->type;
						if (type == typeid(int))
							entry << *static_cast<const int*>(view.field_ptr);
						else if (type == typeid(double))
							entry << *static_cast<const double*>(view.field_ptr);
						else if (type == typeid(FixedString64))
							entry << "\"" << static_cast<const FixedString64*>(view.field_ptr)->c_str() << "\"";
						else if (type == typeid(FixedString128))
							entry << "\"" << static_cast<const FixedString128*>(view.field_ptr)->c_str() << "\"";
						else
							entry << "<unsupported>";
					}

					if (view.struct_info == view.instance->type->input_struct)
						input_entries.push_back(entry.str());
					else
						output_entries.push_back(entry.str());
				});

			row.inputs = input_entries.empty() ? "-" : join(input_entries, "\n");
			row.outputs = output_entries.empty() ? "-" : join(output_entries, "\n");

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
