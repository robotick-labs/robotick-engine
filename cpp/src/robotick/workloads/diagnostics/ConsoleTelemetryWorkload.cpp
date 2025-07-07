// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
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

	ROBOTICK_REGISTER_STRUCT_BEGIN(ConsoleTelemetryConfig)
	ROBOTICK_STRUCT_FIELD(ConsoleTelemetryConfig, bool, enable_pretty_print)
	ROBOTICK_STRUCT_FIELD(ConsoleTelemetryConfig, bool, enable_unicode)
	ROBOTICK_STRUCT_FIELD(ConsoleTelemetryConfig, bool, enable_demo)
	ROBOTICK_REGISTER_STRUCT_END(ConsoleTelemetryConfig)

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
			row.name = info.seed->unique_name.c_str();

			std::vector<std::string> config_entries;
			std::vector<std::string> input_entries;
			std::vector<std::string> output_entries;

			ROBOTICK_ASSERT(engine != nullptr && "Engine should have been set and checked by now");

			WorkloadFieldsIterator::for_each_field_in_workload(*engine,
				info,
				&mirror_buffer,
				[&](const WorkloadFieldView& view)
				{
					std::ostringstream entry;
					entry << view.field_info->name.c_str();

					if (view.subfield_info)
						entry << "." << view.subfield_info->name.c_str();

					entry << "=";

					if (view.subfield_info)
					{
						ROBOTICK_ASSERT(mirror_buffer.contains_object(view.field_ptr, view.subfield_info->find_type_descriptor()->size));

						const TypeId& type = view.subfield_info->type_id;
						if (type == GET_TYPE_ID(int))
							entry << *static_cast<const int*>(view.field_ptr);
						else if (type == GET_TYPE_ID(double))
							entry << *static_cast<const double*>(view.field_ptr);
						else if (type == GET_TYPE_ID(FixedString64))
							entry << "\"" << static_cast<const FixedString64*>(view.field_ptr)->c_str() << "\"";
						else if (type == GET_TYPE_ID(FixedString128))
							entry << "\"" << static_cast<const FixedString128*>(view.field_ptr)->c_str() << "\"";
						else
							entry << "<?>";
					}
					else
					{
						ROBOTICK_ASSERT(mirror_buffer.contains_object(view.field_ptr, view.field_info->find_type_descriptor()->size));

						// fallback for top-level (non-blackboard) fields
						const TypeId& type = view.field_info->type_id;
						if (type == GET_TYPE_ID(int))
							entry << *static_cast<const int*>(view.field_ptr);
						else if (type == GET_TYPE_ID(double))
							entry << *static_cast<const double*>(view.field_ptr);
						else if (type == GET_TYPE_ID(FixedString64))
							entry << "\"" << static_cast<const FixedString64*>(view.field_ptr)->c_str() << "\"";
						else if (type == GET_TYPE_ID(FixedString128))
							entry << "\"" << static_cast<const FixedString128*>(view.field_ptr)->c_str() << "\"";
						else
							entry << "<?>";
					}

					if (view.struct_info == view.workload_info->workload_descriptor->config_desc)
						config_entries.push_back(entry.str());
					else if (view.struct_info == view.workload_info->workload_descriptor->inputs_desc)
						input_entries.push_back(entry.str());
					else
						output_entries.push_back(entry.str());
				});

			row.config = config_entries.empty() ? "-" : join(config_entries, "\n");
			row.inputs = input_entries.empty() ? "-" : join(input_entries, "\n");
			row.outputs = output_entries.empty() ? "-" : join(output_entries, "\n");

			constexpr double NanosecondsPerMillisecond = 1e6;

			row.tick_duration_ms = NanosecondsPerMillisecond * (double)info.mutable_stats.last_tick_duration_ns;
			row.tick_delta_ms = NanosecondsPerMillisecond * (double)info.mutable_stats.last_time_delta_ns;
			row.goal_interval_ms = info.seed->tick_rate_hz > 0.0 ? 1000.0 / info.seed->tick_rate_hz : -1.0;
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

		ConsoleTelemetryWorkload(){};
		~ConsoleTelemetryWorkload(){};

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

				std::ostringstream config_oss;
				std::ostringstream input_oss;
				std::ostringstream output_oss;
				config_oss << "config_" << i << "=" << val_dist(gen);
				input_oss << "input_" << i << "=" << val_dist(gen);
				output_oss << "output_" << i << "=" << val_dist(gen);

				rows.push_back(ConsoleTelemetryRow{"DummyType" + std::to_string(i),
					"Workload" + std::to_string(i),
					config_oss.str(),
					input_oss.str(),
					output_oss.str(),
					tick_ms,
					goal_ms,
					percent});
			}

			return rows;
		}

		void set_engine(const Engine& engine)
		{
			if (!collector)
				collector = std::make_unique<ConsoleTelemetryCollector>();

			collector->set_engine(engine);
		}

		void tick(const TickInfo&)
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

	ROBOTICK_REGISTER_WORKLOAD(ConsoleTelemetryWorkload, ConsoleTelemetryConfig);

} // namespace robotick
