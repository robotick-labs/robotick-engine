// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/model/ModelLoader.h"

#ifdef ROBOTICK_ENABLE_MODEL_LOAD_WIP

#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/model/WorkloadSeed.h"

#include <cstdio>
#include <cstring>

namespace robotick
{
	namespace
	{
		bool starts_with(const char* str, const char* prefix)
		{
			return std::strncmp(str, prefix, std::strlen(prefix)) == 0;
		}

		constexpr int max_line = 512;

		bool parse_line(char* line, FixedString32& key_out, FixedString64& value_out)
		{
			char* colon = std::strchr(line, ':');
			if (!colon)
				return false;

			*colon = '\0';
			key_out = line;

			char* value_start = colon + 1;
			while (*value_start == ' ' || *value_start == '\"')
				value_start++;

			char* end = value_start + std::strlen(value_start) - 1;
			while (end > value_start && (*end == '\n' || *end == '\r' || *end == '\"'))
			{
				*end = '\0';
				end--;
			}

			value_out = value_start;
			return true;
		}
		const WorkloadSeed* find_workload_seed(Model& model, const char* workload_name)
		{
			for (const WorkloadSeed& seed : model.get_workload_seeds_storage())
			{
				if (seed.name == workload_name)
				{
					return &seed;
				}
			}

			return nullptr;
		}
	} // namespace

	bool ModelLoader::load_from_yaml(Model& model, const char* yaml_path, bool auto_finalize_and_validate)
	{
		FILE* file = std::fopen(yaml_path, "r");
		if (!file)
			return false;

		char line_buffer[max_line];

		enum class Section
		{
			None,
			Workloads,
			Connections
		} section = Section::None;

		FixedString64 root_workload_name;

		WorkloadSeed* current_workload = nullptr;

		while (std::fgets(line_buffer, max_line, file))
		{
			char* line = &line_buffer[0];

			// Strip leading whitespace
			while (*line == ' ' || *line == '\t')
				++line;

			if (*line == '\0' || *line == '\n' || *line == '#')
				continue;

			if (starts_with(line, "workloads:"))
			{
				section = Section::Workloads;
				continue;
			}
			else if (starts_with(line, "data_connections:"))
			{
				section = Section::Connections;
				current_workload = nullptr;
				continue;
			}
			else if (starts_with(line, "root:"))
			{
				current_workload = nullptr;
				FixedString32 key;
				FixedString64 value;
				if (parse_line(line, key, value))
				{
					root_workload_name = value;
					// remember it and set it at end, so order of data in yaml isn't as important as in code
					// (e.g. specified workload might not yet have been defined in data)
				}
				continue;
			}

			if (section == Section::Workloads)
			{
				if (starts_with(line, "- name:"))
				{
					current_workload = &(model.add());

					FixedString32 key;
					FixedString64 value;
					if (parse_line(line + 2, key, value))
						current_workload->set_name(value);
				}
				else if (current_workload && starts_with(line, "type:"))
				{
					FixedString32 key;
					FixedString64 value;
					if (parse_line(line, key, value))
						current_workload->set_type_name(value);
				}
				else if (current_workload && starts_with(line, "tick_rate_hz:"))
				{
					FixedString32 key;
					FixedString64 value;
					if (parse_line(line, key, value))
						current_workload->tick_rate_hz = std::atof(value.c_str());
				}
				else if (current_workload && (starts_with(line, "- entry_") || starts_with(line, "- ")))
				{
					FixedString32 key;
					FixedString64 value;
					if (parse_line(line + 2, key, value))
					{
						if (current_workload->config.capacity() == 0 && current_workload->inputs.capacity() == 0)
							current_workload->config.reserve(4); // will switch later

						auto entry = WorkloadSeed::Entry{key, value};
						if (starts_with(key.c_str(), "entry_"))
							current_workload->config.push_back(entry);
						else
							current_workload->inputs.push_back(entry);
					}
				}
				else if (current_workload && starts_with(line, "children:"))
				{
					// Expect format like: children: [A, B]
					char* bracket = std::strchr(line, '[');
					if (bracket)
					{
						char* token = std::strtok(bracket + 1, ",]");
						while (token)
						{
							while (*token == ' ' || *token == '\t')
								++token;
							char* end = token + std::strlen(token) - 1;
							while (end > token && (*end == '\n' || *end == '\r' || *end == ' '))
							{
								*end = '\0';
								--end;
							}
							current_workload->child_names.emplace_back(token);
							token = std::strtok(nullptr, ",]");
						}
					}
				}
			}
			else if (section == Section::Connections && starts_with(line, "- source:"))
			{
				state.connections.emplace_back();
				ParsedConnection& conn = state.connections.back();

				FixedString32 key;
				FixedString64 value;
				if (parse_line(line + 2, key, value))
					conn.source = value;

				std::fgets(line, max_line, file); // next line should be dest
				if (parse_line(line, key, value))
					conn.dest = value;
			}
		}

		std::fclose(file);

		// Resolve children from names already stored in each seed
		const auto& seeds = model.get_workload_seeds();
		for (WorkloadSeed* seed : seeds)
		{
			if (!seed->child_names.empty())
			{
				HeapVector<const WorkloadSeed*> resolved_children;
				for (const FixedString32& name : seed->child_names)
				{
					const WorkloadSeed* child = model.find_seed_by_name(name.c_str());
					if (!child)
						return false;
					resolved_children.push_back(child);
				}
				seed->set_children(resolved_children);
			}
		}

		// Resolve data connections
		for (const auto& conn : state.connections)
			model.connect(conn.source.c_str(), conn.dest.c_str());

		// Set root workload
		if (!root_workload_name.size())
		{
			ROBOTICK_WARNING("Model yaml '%s' needs to specify a root workload", yaml_path);
			return false;
		}

		const WorkloadSeed* root = find_workload_seed(model, root_workload_name.c_str());
		if (!root)
		{
			ROBOTICK_WARNING("Model yaml '%s' specifies an invalid a root workload '%s'", yaml_path, root_workload_name.c_str());
			return false;
		}

		model.set_root_workload(*root, auto_finalize_and_validate);

		return true;
	}
} // namespace robotick

#endif // #ifdef ROBOTICK_ENABLE_MODEL_LOAD_WIP
