// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/ConsoleTelemetryTable.h"
#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include <cassert> // assert
#include <random>  // std::random_device, std::mt19937, std::uniform_real_distribution
#include <sstream> // std::ostringstream
#include <string>  // std::string, std::to_string
#include <vector>  // std::vector

namespace robotick
{
	struct ConsoleTelemetryConfig
	{
		bool enable_pretty_print = true;
		bool enable_unicode = true;
		bool enable_demo = false;
	};

	ROBOTICK_BEGIN_FIELDS(ConsoleTelemetryConfig)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, bool, enable_pretty_print)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, bool, enable_unicode)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, bool, enable_demo)
	ROBOTICK_END_FIELDS()

	class ConsoleTelemetryCollector
	{
	  public:
		void set_engine(const Engine& engine_in)
		{
			engine = &engine_in;
			rows.reserve(engine->get_all_instance_info().size());

			mirror_buffer.create_mirror_from(engine->get_workloads_buffer());
		}

		const Engine* get_engine() const { return engine; };

		std::vector<ConsoleTelemetryRow> collect_rows()
		{
			rows.clear(); // clear every time we're called (else we'll keep accumulating rows)

			if (!engine)
			{
				return rows;
			}

			ROBOTICK_ASSERT(rows.capacity() == engine->get_all_instance_info().size() &&
							"Correct storage for rows should have been reserved when engine was set. Workload-count is fixed after startup.");

			const WorkloadInstanceInfo* root = engine->get_root_instance_info();
			if (!root)
			{
				return rows; // nothing to show if no root has been set - don't assume this is an accident
			}

			mirror_buffer.update_mirror_from(engine->get_workloads_buffer());

			visit_all(*root, 0, rows);
			return rows;
		}

	  private:
		void visit_all(const WorkloadInstanceInfo& info, size_t depth, std::vector<ConsoleTelemetryRow>& rows)
		{
			auto& row = rows.emplace_back();
			populate_row(row, depth, info);
			for (auto* child : info.children)
			{
				if (child)
				{
					visit_all(*child, depth + 1, rows);
				}
			}
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
			row.type = depth_prefix(depth, info.type->name.c_str());
			row.name = info.unique_name;

			std::vector<std::string> input_entries;
			std::vector<std::string> output_entries;

			ROBOTICK_ASSERT(engine != nullptr && "Engine should have been set and checked by now");

			WorkloadFieldsIterator::for_each_field_in_workload(*engine, info, &mirror_buffer,
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
						ROBOTICK_ASSERT(mirror_buffer.contains_object(view.field_ptr, view.subfield->size));

						const TypeId& type = view.subfield->type;
						if (type == GET_TYPE_ID(int))
							entry << *static_cast<const int*>(view.field_ptr);
						else if (type == GET_TYPE_ID(double))
							entry << *static_cast<const double*>(view.field_ptr);
						else if (type == GET_TYPE_ID(FixedString64))
							entry << "\"" << static_cast<const FixedString64*>(view.field_ptr)->c_str() << "\"";
						else if (type == GET_TYPE_ID(FixedString128))
							entry << "\"" << static_cast<const FixedString128*>(view.field_ptr)->c_str() << "\"";
						else
							entry << "<unsupported>";
					}
					else
					{
						ROBOTICK_ASSERT(mirror_buffer.contains_object(view.field_ptr, view.field->size));

						// fallback for top-level (non-blackboard) fields
						const TypeId& type = view.field->type;
						if (type == GET_TYPE_ID(int))
							entry << *static_cast<const int*>(view.field_ptr);
						else if (type == GET_TYPE_ID(double))
							entry << *static_cast<const double*>(view.field_ptr);
						else if (type == GET_TYPE_ID(FixedString64))
							entry << "\"" << static_cast<const FixedString64*>(view.field_ptr)->c_str() << "\"";
						else if (type == GET_TYPE_ID(FixedString128))
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

	  private:
		std::vector<ConsoleTelemetryRow> rows;

		const Engine* engine = nullptr;
		WorkloadsBuffer mirror_buffer; // Provides a local copy of the engine's WorkloadsBuffer to reduce temporal aliasing.
									   // THREADING: The update_mirror_from() operation is not atomic. Race conditions may occur
									   // if other threads modify the source buffer during copying. This is acceptable
									   // for diagnostic telemetry but should not be relied upon for critical operations.
	};

	struct ConsoleTelemetryWorkload
	{
		ConsoleTelemetryConfig config;
		std::unique_ptr<ConsoleTelemetryCollector> collector;

		ConsoleTelemetryWorkload() {};
		~ConsoleTelemetryWorkload() {};

		static std::vector<ConsoleTelemetryRow> collect_console_telemetry_rows_demo(const Engine&)
		{
			std::vector<ConsoleTelemetryRow> rows;

			// Random distributions for generating demo telemetry data:

			static std::random_device rd;
			static std::mt19937 gen(rd());
			static std::uniform_real_distribution<> tick_dist(0.1, 5.0);
			static std::uniform_real_distribution<> goal_dist(1.0, 5.0);
			static std::uniform_real_distribution<> val_dist(0.0, 100.0);

			for (int i = 0; i < 3; ++i)
			{
				double tick_ms = tick_dist(gen);
				double goal_ms = goal_dist(gen);
				double percent = (tick_ms / goal_ms) * 100.0;

				std::ostringstream input_oss;
				std::ostringstream output_oss;
				input_oss << "input_" << i << "=" << val_dist(gen);
				output_oss << "output_" << i << "=" << val_dist(gen);

				rows.push_back(ConsoleTelemetryRow{
					"DummyType" + std::to_string(i), "Workload" + std::to_string(i), input_oss.str(), output_oss.str(), tick_ms, goal_ms, percent});
			}

			return rows;
		}

		void set_engine(const Engine& engine)
		{
			if (!collector)
				collector = std::make_unique<ConsoleTelemetryCollector>();

			collector->set_engine(engine);
		}

		void tick(double)
		{
			// Note - when using ConsoleTelemetryWorkload it is strongly recommended to run it at 5-10Hz max.
			// This avoids overwhelming stdout and dominating frame time, even without pretty printing.
			// To help mitigate this, printing is built as a single string and flushed once per tick.

			ROBOTICK_ASSERT(collector && collector->get_engine() != nullptr && "ConsoleTelemetryWorkload - engine should never be null during tick");
			// ^- we should never reach the point of ticking without set_engine having been called.
			//  - we also assume that any given workload-instance can only ever be part of a single
			//    engine, and that the lifespan of the engine is longer than the workloads that are
			//    owned and created/destroyed by the engine.

			auto rows = config.enable_demo ? collect_console_telemetry_rows_demo(*collector->get_engine()) : collector->collect_rows();

			print_console_telemetry_table(rows, config.enable_pretty_print, config.enable_unicode);
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(ConsoleTelemetryWorkload, ConsoleTelemetryConfig);

} // namespace robotick
