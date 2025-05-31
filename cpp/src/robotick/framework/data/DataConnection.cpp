// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
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

	struct DataConnectionUtils
	{
		static bool is_valid_section(const std::string& s) { return (s == "inputs" || s == "outputs" || s == "config"); }

		static ParsedFieldPath parse_field_path(const std::string& raw)
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

		static const StructRegistryEntry* get_struct_entry(const WorkloadInstanceInfo& instance, const std::string& section, size_t& out_offset)
		{
			const auto* type = instance.type;
			if (!type)
			{
				throw std::runtime_error("Missing type info for workload: " + instance.unique_name);
			}

			const StructRegistryEntry* result = nullptr;

			if (section == "inputs" && type->input_struct != nullptr)
			{
				out_offset = type->input_struct->offset_within_workload;
				result = type->input_struct;
			}
			else if (section == "outputs" && type->output_struct != nullptr)
			{
				out_offset = type->output_struct->offset_within_workload;
				result = type->output_struct;
			}
			else if (section == "config" && type->config_struct != nullptr)
			{
				out_offset = type->config_struct->offset_within_workload;
				result = type->config_struct;
			}
			else
			{
				throw std::runtime_error("Invalid section: " + section);
			}

			assert((result == nullptr || out_offset != OFFSET_UNBOUND) && "StructRegistryEntry with unbound offset should not exist");
			return result;
		}

		static const FieldInfo* find_field(const StructRegistryEntry* struct_entry, const std::string& field_name)
		{
			if (!struct_entry)
			{
				return nullptr;
			}

			const auto& it_found = struct_entry->field_from_name.find(field_name);
			if (it_found == struct_entry->field_from_name.end())
			{
				return nullptr;
			}

			const FieldInfo* found_field = it_found->second;
			return found_field;
		}

		static const BlackboardFieldInfo* resolve_blackboard_field_ptr(WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& inst,
			const StructRegistryEntry& struct_info, const FieldInfo& blackboard_field, const std::string& blackboard_subfield_name)
		{
			if (blackboard_field.type != std::type_index(typeid(Blackboard)))
			{
				return nullptr;
			}

			const Blackboard& blackboard = blackboard_field.get_data<Blackboard>(workloads_buffer, inst, struct_info);
			return blackboard.get_field_info(blackboard_subfield_name);
		}
	};

	std::vector<DataConnectionInfo> DataConnectionsFactory::create(
		WorkloadsBuffer& workloads_buffer, const std::vector<DataConnectionSeed>& seeds, const std::vector<WorkloadInstanceInfo>& instances)
	{
		std::vector<DataConnectionInfo> results;
		std::unordered_set<std::string> seen_destinations;

		std::unordered_map<std::string_view, const WorkloadInstanceInfo*> idx;
		for (const auto& inst : instances)
			idx.emplace(inst.unique_name.c_str(), &inst);

		for (const auto& seed : seeds)
		{
			ParsedFieldPath src = DataConnectionUtils::parse_field_path(seed.source_field_path);
			ParsedFieldPath dst = DataConnectionUtils::parse_field_path(seed.dest_field_path);

			const auto src_it = idx.find(src.workload_name.c_str());
			const auto dst_it = idx.find(dst.workload_name.c_str());
			const WorkloadInstanceInfo* src_inst = src_it != idx.end() ? src_it->second : nullptr;
			const WorkloadInstanceInfo* dst_inst = dst_it != idx.end() ? dst_it->second : nullptr;

			if (!src_inst)
			{
				throw std::runtime_error("Unknown source workload: " + std::string(src.workload_name.c_str()));
			}

			if (!dst_inst)
			{
				throw std::runtime_error("Unknown destination workload: " + std::string(dst.workload_name.c_str()));
			}

			// Lookup struct + field for source
			size_t src_struct_offset = OFFSET_UNBOUND;
			const StructRegistryEntry* src_struct = DataConnectionUtils::get_struct_entry(*src_inst, src.section_name.c_str(), src_struct_offset);
			const FieldInfo* src_field = DataConnectionUtils::find_field(src_struct, src.field_path[0].c_str());
			if (!src_field)
			{
				throw std::runtime_error("Source field not found: " + seed.source_field_path);
			}

			assert(src_struct_offset != OFFSET_UNBOUND && "Src struct offset should have definitely been set by now");

			const uint8_t* src_ptr = src_field->get_data_ptr(workloads_buffer, *src_inst, *src_struct);
			std::type_index src_type = src_field->type;
			size_t src_size = src_field->size;

			if (src.field_path.size() == 2)
			{
				const BlackboardFieldInfo* src_blackboard_field = DataConnectionUtils::resolve_blackboard_field_ptr(
					workloads_buffer, *src_inst, *src_struct, *src_field, src.field_path[1].c_str());

				if (!src_blackboard_field)
				{
					throw std::runtime_error("Source subfield not found: " + seed.source_field_path);
				}

				assert(workloads_buffer.contains_object(src_ptr, src_size) && "Blackboard should be within supplied workloads-buffer");

				const Blackboard* blackboard = static_cast<const Blackboard*>((void*)src_ptr);
				const size_t blackboard_datablock_offset = blackboard->get_datablock_offset();

				assert(blackboard_datablock_offset != OFFSET_UNBOUND && "Blackboard data-block offset should have been set by now");

				src_ptr = src_ptr + blackboard_datablock_offset + src_blackboard_field->offset_from_datablock;
				src_type = src_blackboard_field->type;
				src_size = src_blackboard_field->size;
			}

			assert(workloads_buffer.contains_object(src_ptr, src_size) && "Source Field pointer should be within supplied workloads-buffer");

			// Lookup struct + field for dest
			size_t dst_struct_offset = OFFSET_UNBOUND;
			const StructRegistryEntry* dst_struct = DataConnectionUtils::get_struct_entry(*dst_inst, dst.section_name.c_str(), dst_struct_offset);
			const FieldInfo* dst_field = DataConnectionUtils::find_field(dst_struct, dst.field_path[0].c_str());
			if (!dst_field || dst_struct_offset == OFFSET_UNBOUND)
			{
				throw std::runtime_error("Destination field not found: " + seed.dest_field_path);
			}

			assert(dst_struct_offset != OFFSET_UNBOUND && "Dest struct offset should have definitely been set by now");

			uint8_t* dst_ptr = dst_field->get_data_ptr(workloads_buffer, *dst_inst, *dst_struct);
			std::type_index dst_type = dst_field->type;
			size_t dst_size = dst_field->size;

			if (dst.field_path.size() == 2)
			{
				const BlackboardFieldInfo* dst_blackboard_field = DataConnectionUtils::resolve_blackboard_field_ptr(
					workloads_buffer, *dst_inst, *dst_struct, *dst_field, dst.field_path[1].c_str());

				if (!dst_blackboard_field)
				{
					throw std::runtime_error("Dest subfield not found: " + seed.dest_field_path);
				}

				auto* blackboard = reinterpret_cast<Blackboard*>(dst_ptr);
				dst_ptr = dst_ptr + blackboard->get_datablock_offset() + dst_blackboard_field->offset_from_datablock;
				dst_type = dst_blackboard_field->type;
				dst_size = dst_blackboard_field->size;
			}

			assert(workloads_buffer.contains_object(dst_ptr, dst_size) && "Destination Field pointer should be within supplied workloads-buffer");

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
