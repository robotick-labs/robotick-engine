// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/Blackboard.h"
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

		if (tokens.size() < 3 || tokens.size() > 4)
		{
			throw FieldPathParseError("Expected format <workload>.<section>.<field> or <workload>.<section>.<field>.<subfield>: " + raw);
		}

		if (!is_valid_section(tokens[1]))
		{
			throw FieldPathParseError("Invalid section '" + tokens[1] + "' in path: " + raw);
		}

		const bool has_subfield = tokens.size() == 4;
		if (has_subfield)
		{
			return ParsedFieldPath{FixedString64(tokens[0]), FixedString64(tokens[1]), {FixedString64(tokens[2]), FixedString64(tokens[3])}};
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

	const robotick::BlackboardField* resolve_blackboard_field_ptr(const robotick::WorkloadInstanceInfo& inst,
		const robotick::FieldInfo& blackboard_field, const std::string& blackboard_subfield_name, size_t struct_offset)
	{
		if (blackboard_field.type != std::type_index(typeid(robotick::Blackboard)))
		{
			return nullptr;
		}

		const robotick::Blackboard* blackboard = reinterpret_cast<const robotick::Blackboard*>(inst.ptr + struct_offset + blackboard_field.offset);
		if (!blackboard)
		{
			return nullptr;
		}

		return blackboard->get_schema_field(blackboard_subfield_name);
	}

	std::vector<DataConnectionInfo> DataConnectionsFactory::create(
		const std::vector<DataConnectionSeed>& seeds, const std::vector<WorkloadInstanceInfo>& instances)
	{
		std::vector<DataConnectionInfo> results;
		std::unordered_set<std::string> seen_destinations;

		std::unordered_map<std::string_view, const WorkloadInstanceInfo*> idx;
		for (const auto& inst : instances)
			idx.emplace(inst.unique_name.c_str(), &inst);

		for (const auto& seed : seeds)
		{
			ParsedFieldPath src = parse_field_path(seed.source_field_path);
			ParsedFieldPath dst = parse_field_path(seed.dest_field_path);

			const auto src_it = idx.find(src.workload_name.c_str());
			const auto dst_it = idx.find(dst.workload_name.c_str());
			const WorkloadInstanceInfo* src_inst = src_it != idx.end() ? src_it->second : nullptr;
			const WorkloadInstanceInfo* dst_inst = dst_it != idx.end() ? dst_it->second : nullptr;

			if (!src_inst || !src_inst->ptr)
			{
				throw std::runtime_error("Unknown source workload: " + std::string(src.workload_name.c_str()));
			}
			if (!dst_inst || !dst_inst->ptr)
			{
				throw std::runtime_error("Unknown destination workload: " + std::string(dst.workload_name.c_str()));
			}

			// Lookup struct + field for source
			size_t src_offset = 0;
			const StructRegistryEntry* src_struct = get_struct_entry(*src_inst, src.section_name.c_str(), src_offset);
			const FieldInfo* src_field = find_field(src_struct, src.field_path[0].c_str());
			if (!src_field)
			{
				throw std::runtime_error("Source field not found: " + seed.source_field_path);
			}

			if (!src_inst->ptr)
				throw std::runtime_error("Workload '" + src_inst->unique_name + "' data buffer is null – cannot resolve connection");

			const uint8_t* src_ptr = src_inst->ptr + src_offset + src_field->offset;
			std::type_index src_type = src_field->type;
			size_t src_size = src_field->size;

			if (src.field_path.size() == 2)
			{
				const BlackboardField* src_blackboard_field =
					resolve_blackboard_field_ptr(*src_inst, *src_field, src.field_path[1].c_str(), src_offset);

				if (!src_blackboard_field)
				{
					throw std::runtime_error("Source subfield not found: " + seed.source_field_path);
				}

				const auto* blackboard = reinterpret_cast<const Blackboard*>(src_ptr);
				src_ptr = blackboard->get_base_ptr() + src_blackboard_field->offset;
				src_type = src_blackboard_field->type;
				src_size = src_blackboard_field->size;
			}

			// Lookup struct + field for dest
			size_t dst_offset = 0;
			const StructRegistryEntry* dst_struct = get_struct_entry(*dst_inst, dst.section_name.c_str(), dst_offset);
			const FieldInfo* dst_field = find_field(dst_struct, dst.field_path[0].c_str());
			if (!dst_field)
			{
				throw std::runtime_error("Destination field not found: " + seed.dest_field_path);
			}

			uint8_t* dst_ptr = dst_inst->ptr + dst_offset + dst_field->offset;
			std::type_index dst_type = dst_field->type;
			size_t dst_size = dst_field->size;

			if (dst.field_path.size() == 2)
			{
				const BlackboardField* dst_blackboard_field =
					resolve_blackboard_field_ptr(*dst_inst, *dst_field, dst.field_path[1].c_str(), dst_offset);

				if (!dst_blackboard_field)
				{
					throw std::runtime_error("Dest subfield not found: " + seed.dest_field_path);
				}

				auto* blackboard = reinterpret_cast<Blackboard*>(dst_ptr);
				dst_ptr = blackboard->get_base_ptr() + dst_blackboard_field->offset;
				dst_type = dst_blackboard_field->type;
				dst_size = dst_blackboard_field->size;
			}

			// Validate type match
			if (src_type != dst_type)
			{
				throw std::runtime_error("Type mismatch between source and dest: " + seed.source_field_path + " vs. " + seed.dest_field_path);
			}

			// Validate size match
			if (src_size != dst_size)
			{
				std::ostringstream oss;
				oss << "Size mismatch (" << src_size << " vs " << dst_size << ") between " << seed.source_field_path << " and "
					<< seed.dest_field_path;
				throw std::runtime_error(oss.str());
			}

			std::string dst_key = std::string(dst.workload_name.c_str()) + "." + dst.section_name.c_str() + "." + dst.field_path[0].c_str();
			if (dst.field_path.size() == 2)
			{
				dst_key = dst_key + "." + dst.field_path[1].c_str();
			}

			if (!seen_destinations.insert(dst_key).second)
			{
				throw std::runtime_error("Duplicate destination field: " + dst_key);
			}

			// results.push_back(DataConnectionInfo{src_ptr, dst_ptr, src_field->size, src_field->type});
			results.push_back(DataConnectionInfo{seed, src_ptr, dst_ptr, src_inst, dst_inst, src_size, src_type});
		}

		return results;
	}

} // namespace robotick
