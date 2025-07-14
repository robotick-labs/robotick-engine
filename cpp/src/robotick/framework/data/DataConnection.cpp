// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"

#include "robotick/api_base.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/WorkloadSeed.h"

namespace robotick
{
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
			const TypeDescriptor* field_type_desc = field->find_type_descriptor();
			if (!field_type_desc)
				ROBOTICK_FATAL_EXIT("Field '%s' in path '%s' has unknown type", field_token.c_str(), path);
			size_t size = field_type_desc->size;

			// Step 4: optional subfield
			if (*path_cursor != '\0') // There’s still more of the path
			{
				const FixedString64 subfield_token = extract_next_token(path_cursor);

				const StructDescriptor* struct_desc = field_type_desc->get_struct_desc();
				if (!struct_desc)
				{
					if (const DynamicStructDescriptor* dynamic_struct_desc = field_type_desc->get_dynamic_struct_desc())
					{
						struct_desc = dynamic_struct_desc->get_struct_descriptor(ptr);
					}
				}

				if (struct_desc)
				{
					const FieldDescriptor* sub_field = struct_desc->find_field(subfield_token.c_str());
					if (!sub_field)
						ROBOTICK_FATAL_EXIT("Subfield '%s' not found in path: %s", subfield_token.c_str(), path);

					ptr = (uint8_t*)sub_field->get_data_ptr((void*)ptr);
					type = sub_field->type_id;
					const TypeDescriptor* sub_field_type_desc = sub_field->find_type_descriptor();
					if (!sub_field_type_desc)
						ROBOTICK_FATAL_EXIT("Subfield '%s' in path '%s' has unknown type", subfield_token.c_str(), path);
					size = sub_field_type_desc->size;
				}
				else
				{
					ROBOTICK_FATAL_EXIT("Field '%s' in path '%s' has no sub-field", field_token.c_str(), path);
				}
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

	std::tuple<void*, size_t, const FieldDescriptor*> DataConnectionUtils::find_field_info(const Engine& engine, const char* path)
	{
		const WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer();
		const Map<const char*, WorkloadInstanceInfo*>& instances = engine.get_all_instance_info_map();
		const char* path_cursor = path;

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
			ROBOTICK_WARNING("Invalid section '%s' in field path: %s", section_token.c_str(), path);
			return {nullptr, 0, nullptr};
		}

		const FixedString64 field_token = extract_next_token(path_cursor);
		const FieldDescriptor* field = DataConnectionHelpers::find_field(struct_type, field_token.c_str());
		if (!field)
		{
			ROBOTICK_WARNING("Field '%s' not found in path: %s", field_token.c_str(), path);
			return {nullptr, 0, nullptr};
		}

		const TypeDescriptor* field_type_desc = field->find_type_descriptor();
		if (!field_type_desc)
			ROBOTICK_FATAL_EXIT("Field '%s' in path '%s' has unknown type", field_token.c_str(), path);

		void* base_ptr = field->get_data_ptr(const_cast<WorkloadsBuffer&>(workloads_buffer), *workload_info, *struct_type, struct_offset);
		size_t size = field_type_desc->size;

		// optional field.subfield
		if (*path_cursor != '\0')
		{
			const FixedString64 subfield_token = extract_next_token(path_cursor);

			const StructDescriptor* struct_desc = field_type_desc->get_struct_desc();
			if (!struct_desc)
			{
				if (const DynamicStructDescriptor* dynamic_struct_desc = field_type_desc->get_dynamic_struct_desc())
				{
					struct_desc = dynamic_struct_desc->get_struct_descriptor(base_ptr);
				}
			}

			if (struct_desc)
			{
				const FieldDescriptor* sub_field = struct_desc->find_field(subfield_token.c_str());
				if (!sub_field)
					ROBOTICK_FATAL_EXIT("Subfield '%s' not found in path: %s", subfield_token.c_str(), path);

				base_ptr = (uint8_t*)sub_field->get_data_ptr(base_ptr);
				const TypeDescriptor* sub_field_type_desc = sub_field->find_type_descriptor();
				if (!sub_field_type_desc)
					ROBOTICK_FATAL_EXIT("Subfield '%s' in path '%s' has unknown type", subfield_token.c_str(), path);

				size = sub_field_type_desc->size;
				field = sub_field;
			}
			else
			{
				ROBOTICK_FATAL_EXIT("Field '%s' in path '%s' has no sub-field", field_token.c_str(), path);
			}
		}

		return {base_ptr, size, field};
	}

} // namespace robotick
