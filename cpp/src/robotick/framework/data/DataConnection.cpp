// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace robotick
{

	FieldPathParseError::FieldPathParseError(const std::string& msg) : std::runtime_error(msg)
	{
	}

	bool is_valid_section(const std::string& s)
	{
		return (s == "inputs" || s == "outputs" || s == "config");
	}

	ParsedFieldPath parse_field_path(const std::string& raw)
	{
		std::istringstream ss(raw);
		std::string token;
		std::vector<std::string> tokens;

		while (std::getline(ss, token, '.'))
		{
			if (token.empty())
			{
				throw FieldPathParseError("Empty segment in field path: " + raw);
			}
			tokens.push_back(token);
		}

		if (tokens.size() != 3)
		{
			throw FieldPathParseError("Expected format <workload>.<section>.<field>: " + raw);
		}

		if (!is_valid_section(tokens[1]))
		{
			throw FieldPathParseError("Invalid section '" + tokens[1] + "' in path: " + raw);
		}

		return ParsedFieldPath{FixedString64(tokens[0]), FixedString64(tokens[1]), {FixedString64(tokens[2])}};
	}

	const StructRegistryEntry* get_struct_entry(const WorkloadInstanceInfo& instance, const std::string& section, size_t& out_offset)
	{
		const auto* type = instance.type;
		if (!type)
		{
			throw std::runtime_error("Missing type info for workload: " + instance.unique_name);
		}

		if (section == "inputs")
		{
			out_offset = type->input_offset;
			return type->input_struct;
		}
		if (section == "outputs")
		{
			out_offset = type->output_offset;
			return type->output_struct;
		}
		if (section == "config")
		{
			out_offset = type->config_offset;
			return type->config_struct;
		}

		throw std::runtime_error("Invalid section: " + section);
	}

	const FieldInfo* find_field(const StructRegistryEntry* struct_entry, const std::string& field_name)
	{
		if (!struct_entry)
		{
			return nullptr;
		}

		for (const auto& field : struct_entry->fields)
		{
			if (field.name == field_name)
			{
				return &field;
			}
		}

		return nullptr;
	}

	std::vector<DataConnectionInfo> DataConnectionResolver::resolve(
		const std::vector<DataConnectionSeed>& seeds, const std::vector<WorkloadInstanceInfo>& instances)
	{
		std::vector<DataConnectionInfo> results;
		std::unordered_set<std::string> seen_destinations;

		for (const auto& seed : seeds)
		{
			ParsedFieldPath src = parse_field_path(seed.source_path);
			ParsedFieldPath dst = parse_field_path(seed.dest_path);

			// Lookup instances
			const WorkloadInstanceInfo* src_inst = nullptr;
			const WorkloadInstanceInfo* dst_inst = nullptr;

			for (const auto& inst : instances)
			{
				if (inst.unique_name == src.workload_name.c_str())
				{
					src_inst = &inst;
				}
				if (inst.unique_name == dst.workload_name.c_str())
				{
					dst_inst = &inst;
				}
			}

			if (!src_inst)
			{
				throw std::runtime_error("Unknown source workload: " + std::string(src.workload_name.c_str()));
			}
			if (!dst_inst)
			{
				throw std::runtime_error("Unknown destination workload: " + std::string(dst.workload_name.c_str()));
			}

			// Lookup struct + field for source
			size_t src_offset = 0;
			const StructRegistryEntry* src_struct = get_struct_entry(*src_inst, src.section_name.c_str(), src_offset);
			const FieldInfo* src_field = find_field(src_struct, src.field_path[0].c_str());
			if (!src_field)
			{
				throw std::runtime_error("Source field not found: " + seed.source_path);
			}

			// Lookup struct + field for dest
			size_t dst_offset = 0;
			const StructRegistryEntry* dst_struct = get_struct_entry(*dst_inst, dst.section_name.c_str(), dst_offset);
			const FieldInfo* dst_field = find_field(dst_struct, dst.field_path[0].c_str());
			if (!dst_field)
			{
				throw std::runtime_error("Destination field not found: " + seed.dest_path);
			}

			// Validate type match
			if (src_field->type != dst_field->type)
			{
				throw std::runtime_error("Type mismatch between source and dest: " + seed.source_path + " vs. " + seed.dest_path);
			}

			// Validate size match
			if (src_field->size != dst_field->size)
			{
				throw std::runtime_error("Size mismatch between source and dest: " + seed.source_path + " vs. " + seed.dest_path);
			}

			const void* src_ptr = src_inst->ptr + src_offset + src_field->offset;
			void* dst_ptr = dst_inst->ptr + dst_offset + dst_field->offset;

			std::string dst_key = std::string(dst.workload_name.c_str()) + "." + dst.section_name.c_str() + "." + dst.field_path[0].c_str();

			if (!seen_destinations.insert(dst_key).second)
			{
				throw std::runtime_error("Duplicate destination field: " + dst_key);
			}

			results.push_back(DataConnectionInfo{src_ptr, dst_ptr, src_field->size, src_field->type});
		}

		return results;
	}

} // namespace robotick
