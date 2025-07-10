// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"

#include "robotick/api_base.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/WorkloadSeed.h"

namespace robotick
{
	struct ParsedFieldPath
	{
		FixedString64 workload_name;
		FixedString64 section_name; // workload_name/section_name/field_path[0](/field_path[1])
		std::vector<FixedString64> field_path;
	};

	FieldPathParseError::FieldPathParseError(const std::string& msg)
		: std::runtime_error(msg)
	{
	}

	struct DataConnectionHelpers
	{
		static const TypeDescriptor* get_struct_entry(const WorkloadInstanceInfo& instance, const std::string& section, size_t& out_offset)
		{
			const TypeDescriptor* type = instance.type;
			if (!type)
			{
				ROBOTICK_FATAL_EXIT("Missing type info for workload: %s", instance.seed->unique_name.c_str());
			}

			const WorkloadDescriptor* workload_desc = type->get_workload_desc();
			if (!workload_desc)
			{
				ROBOTICK_FATAL_EXIT("Missing workload_desc info for workload: %s", instance.seed->unique_name.c_str());
			}

			const TypeDescriptor* result = nullptr;

			if (section == "inputs" && workload_desc->inputs_desc != nullptr)
			{
				out_offset = workload_desc->inputs_offset;
				result = workload_desc->inputs_desc;
			}
			else if (section == "outputs" && workload_desc->outputs_desc != nullptr)
			{
				out_offset = workload_desc->outputs_offset;
				result = workload_desc->outputs_desc;
			}
			else if (section == "config" && workload_desc->config_desc != nullptr)
			{
				out_offset = workload_desc->config_offset;
				result = workload_desc->config_desc;
			}
			else
			{
				ROBOTICK_FATAL_EXIT("Invalid section: %s", section.c_str());
			}

			ROBOTICK_ASSERT((result == nullptr || out_offset != OFFSET_UNBOUND) && "StructRegistryEntry with unbound offset should not exist");
			return result;
		}

		static const FieldDescriptor* find_field(const TypeDescriptor* struct_entry, const char* field_name)
		{
			if (!struct_entry)
			{
				return nullptr;
			}

			const StructDescriptor* struct_desc = struct_entry->get_struct_desc();
			if (!struct_desc)
			{
				return nullptr;
			}

			const FieldDescriptor* found_field = struct_desc->find_field(field_name);
			return found_field;
		}

		static const FieldDescriptor* resolve_blackboard_field_ptr(WorkloadsBuffer& workloads_buffer,
			const WorkloadInstanceInfo& inst,
			const TypeDescriptor& struct_info,
			const size_t struct_offset,
			const FieldDescriptor& blackboard_field,
			const char* blackboard_subfield_name)
		{
			if (blackboard_field.type_id != GET_TYPE_ID(Blackboard))
			{
				return nullptr;
			}

			ROBOTICK_ASSERT(struct_info.get_struct_desc() != nullptr);

			const Blackboard& blackboard = blackboard_field.get_data<Blackboard>(workloads_buffer, inst, struct_info, struct_offset);
			const StructDescriptor& blackboard_struct_desc = blackboard.get_struct_descriptor();

			const FieldDescriptor* found_field = blackboard_struct_desc.find_field(blackboard_subfield_name);
			return found_field;
		}
	};

	namespace
	{
		struct ResolvedField
		{
			const WorkloadInstanceInfo* workload;
			const void* ptr;
			TypeId type;
			size_t size;
		};

		inline FixedString64 extract_next_token(const char*& path_cursor)
		{
			FixedString64 token;
			size_t i = 0;
			while (*path_cursor && *path_cursor != '.')
			{
				if (i >= token.capacity() - 1)
				{
					ROBOTICK_WARNING("Token too long in path (max %zu): %s", token.capacity() - 1, path_cursor);
					return {};
				}
				token.data[i++] = *path_cursor++;
			}
			token.data[i] = '\0';
			if (*path_cursor == '.')
				++path_cursor; // skip dot
			return token;
		}

		ResolvedField resolve_field_ptr(const char* path, const Map<const char*, WorkloadInstanceInfo*>& instances, WorkloadsBuffer& workloads_buffer)
		{
			const char* path_cursor = path;

			// Step 1: workload
			const FixedString64 workload_token = extract_next_token(path_cursor);
			WorkloadInstanceInfo* const* found_workload_ptr = instances.find(workload_token.c_str());
			const WorkloadInstanceInfo* workload = found_workload_ptr ? *found_workload_ptr : nullptr;
			if (!workload)
				ROBOTICK_FATAL_EXIT("Unknown workload: %s", workload_token.c_str());

			// Step 2: section (config, inputs, outputs)
			const FixedString64 section_token = extract_next_token(path_cursor);
			size_t struct_offset = OFFSET_UNBOUND;
			const TypeDescriptor* struct_type = DataConnectionHelpers::get_struct_entry(*workload, section_token.c_str(), struct_offset);
			if (!struct_type)
				ROBOTICK_FATAL_EXIT("Unknown section '%s' in path: %s", section_token.c_str(), path);

			// Step 3: field
			const FixedString64 field_token = extract_next_token(path_cursor);
			const FieldDescriptor* field = DataConnectionHelpers::find_field(struct_type, field_token.c_str());
			if (!field)
				ROBOTICK_FATAL_EXIT("Field '%s' not found in path: %s", field_token.c_str(), path);

			const uint8_t* ptr = (uint8_t*)field->get_data_ptr(workloads_buffer, *workload, *struct_type, struct_offset);
			TypeId type = field->type_id;
			size_t size = field->find_type_descriptor()->size;

			// Step 4: optional subfield (only if Blackboard)
			if (*path_cursor != '\0') // There’s still more of the path
			{
				const FixedString64 subfield_token = extract_next_token(path_cursor);

				const FieldDescriptor* blackboard_field = DataConnectionHelpers::resolve_blackboard_field_ptr(
					workloads_buffer, *workload, *struct_type, struct_offset, *field, subfield_token.c_str());

				if (!blackboard_field)
					ROBOTICK_FATAL_EXIT("Blackboard subfield '%s' not found in path: %s", subfield_token.c_str(), path);

				const Blackboard* blackboard = static_cast<const Blackboard*>((const void*)ptr);
				ptr = ptr + blackboard->get_datablock_offset() + blackboard_field->offset_within_container;
				type = blackboard_field->type_id;
				size = blackboard_field->find_type_descriptor()->size;
			}

			if (*path_cursor != '\0')
				ROBOTICK_FATAL_EXIT("Too many path components in: %s", path);

			ROBOTICK_ASSERT(workloads_buffer.contains_object(ptr, size) && "Resolved field must be within workloads_buffer");

			return ResolvedField{workload, ptr, type, size};
		}
	} // namespace

	static bool has_connection_to_field(const HeapVector<DataConnectionInfo>& query_connections, const void* query_dest_ptr)
	{
		for (const DataConnectionInfo& query_connection : query_connections)
		{
			if (query_connection.dest_ptr == query_dest_ptr)
			{
				return true;
			}
		}

		return false;
	}

	void DataConnectionUtils::create(HeapVector<DataConnectionInfo>& out_connections,
		WorkloadsBuffer& workloads_buffer,
		const ArrayView<const DataConnectionSeed*>& seeds,
		const Map<const char*, WorkloadInstanceInfo*>& instances)
	{
		size_t connection_index = 0;
		out_connections.initialize(seeds.size());

		for (const DataConnectionSeed* seed_ptr : seeds)
		{
			ROBOTICK_ASSERT(seed_ptr);
			const auto& seed = *seed_ptr;

			const ResolvedField src = resolve_field_ptr(seed.source_field_path.c_str(), instances, workloads_buffer);
			const ResolvedField dst = resolve_field_ptr(seed.dest_field_path.c_str(), instances, workloads_buffer);

			if (src.type != dst.type)
				ROBOTICK_FATAL_EXIT("Type mismatch: %s vs %s", seed.source_field_path.c_str(), seed.dest_field_path.c_str());

			if (src.size != dst.size)
				ROBOTICK_FATAL_EXIT("Size mismatch: %s vs %s", seed.source_field_path.c_str(), seed.dest_field_path.c_str());

			if (has_connection_to_field(out_connections, dst.ptr))
				ROBOTICK_FATAL_EXIT("Duplicate connection to field: %s", seed.dest_field_path.c_str());

			out_connections[connection_index] =
				DataConnectionInfo{&seed, src.ptr, const_cast<void*>(dst.ptr), src.workload, dst.workload, src.size, src.type};

			connection_index++;
		}
	}

	std::tuple<void*, size_t, const FieldDescriptor*> DataConnectionUtils::find_field_info(const Engine& engine, const std::string& path)
	{
		const WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer();
		const Map<const char*, WorkloadInstanceInfo*>& instances = engine.get_all_instance_info_map();
		const char* path_cursor = path.c_str();

		// workload.section.field[.blackboard_field]
		const FixedString64 workload_token = extract_next_token(path_cursor);
		auto* workload_info_ptr = instances.find(workload_token.c_str());
		if (!workload_info_ptr)
		{
			ROBOTICK_WARNING("Unknown workload in field path: %s", workload_token.c_str());
			return {nullptr, 0, nullptr};
		}
		const WorkloadInstanceInfo* workload_info = *workload_info_ptr;

		const FixedString64 section_token = extract_next_token(path_cursor);
		size_t struct_offset = OFFSET_UNBOUND;
		const TypeDescriptor* struct_type = DataConnectionHelpers::get_struct_entry(*workload_info, section_token.c_str(), struct_offset);
		if (!struct_type)
		{
			ROBOTICK_WARNING("Invalid section '%s' in field path: %s", section_token.c_str(), path.c_str());
			return {nullptr, 0, nullptr};
		}

		const FixedString64 field_token = extract_next_token(path_cursor);
		const FieldDescriptor* field = DataConnectionHelpers::find_field(struct_type, field_token.c_str());
		if (!field)
		{
			ROBOTICK_WARNING("Field '%s' not found in path: %s", field_token.c_str(), path.c_str());
			return {nullptr, 0, nullptr};
		}

		void* base_ptr = field->get_data_ptr(const_cast<WorkloadsBuffer&>(workloads_buffer), *workload_info, *struct_type, struct_offset);
		size_t size = field->find_type_descriptor() ? field->find_type_descriptor()->size : 0;

		// optional blackboard.subfield
		if (*path_cursor != '\0')
		{
			const FixedString64 subfield_token = extract_next_token(path_cursor);

			const FieldDescriptor* subfield = DataConnectionHelpers::resolve_blackboard_field_ptr(
				const_cast<WorkloadsBuffer&>(workloads_buffer), *workload_info, *struct_type, struct_offset, *field, subfield_token.c_str());

			if (!subfield)
			{
				ROBOTICK_WARNING("Blackboard subfield '%s' not found in path: %s", subfield_token.c_str(), path.c_str());
				return {nullptr, 0, nullptr};
			}

			const Blackboard* blackboard = static_cast<const Blackboard*>((const void*)base_ptr);
			base_ptr = (uint8_t*)base_ptr + blackboard->get_datablock_offset() + subfield->offset_within_container;
			size = subfield->find_type_descriptor() ? subfield->find_type_descriptor()->size : 0;
			field = subfield;
		}

		return {base_ptr, size, field};
	}

} // namespace robotick
