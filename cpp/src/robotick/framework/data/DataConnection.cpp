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
				return nullptr;

			const StructDescriptor* struct_desc = struct_entry->get_struct_desc();
			if (!struct_desc)
				return nullptr;

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

		// Walks a dotted member path inside an already-addressable container (supports static or dynamic structs).
		// Example: container_ptr=ptr_to_vec2, container_type=Vec2, dotted="x" or "position.x"
		static bool resolve_nested_member(void* container_ptr,
			const TypeDescriptor* container_type,
			const char* dotted,
			void** out_ptr,
			const TypeDescriptor** out_type,
			const FieldDescriptor** out_field)
		{
			if (!container_ptr || !container_type || !dotted || !*dotted)
				return false;

			auto get_struct_desc = [](const TypeDescriptor* td, void* base) -> const StructDescriptor*
			{
				if (const StructDescriptor* s = td->get_struct_desc())
					return s;
				if (const DynamicStructDescriptor* d = td->get_dynamic_struct_desc())
					return d->get_struct_descriptor(base);
				return nullptr;
			};

			const char* cursor = dotted;
			void* cur_ptr = container_ptr;
			const TypeDescriptor* cur_type = container_type;
			const StructDescriptor* cur_struct = get_struct_desc(cur_type, cur_ptr);

			// Iterate tokens until cursor reaches '\0'
			while (*cursor)
			{
				if (!cur_struct)
				{
					ROBOTICK_WARNING("No current struct - remaining items: %s", cursor);
					return false;
				}

				FixedString64 token = extract_next_token(cursor);
				const FieldDescriptor* fld = cur_struct->find_field(token.c_str());
				if (!fld)
				{
					ROBOTICK_WARNING("Could not find field named %s", token.c_str());
					return false;
				}

				void* fld_ptr = fld->get_data_ptr(cur_ptr);
				const TypeDescriptor* fld_type = fld->find_type_descriptor();
				if (!fld_type)
				{
					ROBOTICK_WARNING("Could not find type for field named %s", token.c_str());
					return false;
				}

				cur_ptr = fld_ptr;
				cur_type = fld_type;
				cur_struct = get_struct_desc(cur_type, cur_ptr);

				if (out_field)
					*out_field = fld;
				if (out_ptr)
					*out_ptr = fld_ptr;
				if (out_type)
					*out_type = fld_type;
			}

			return true;
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
			const TypeDescriptor* field_type_desc = field->find_type_descriptor();
			if (!field_type_desc)
				ROBOTICK_FATAL_EXIT("Field '%s' in path '%s' has unknown type '%s'", field_token.c_str(), path, field->type_id.get_debug_name());
			TypeId type = field->type_id;
			size_t size = field_type_desc->size;

			// Step 4: optional subfield chain
			if (*path_cursor != '\0')
			{
				void* sub_ptr = const_cast<uint8_t*>(ptr);
				const TypeDescriptor* sub_type = field_type_desc;
				const FieldDescriptor* sub_desc = nullptr;

				if (!resolve_nested_member(sub_ptr, sub_type, path_cursor, &sub_ptr, &sub_type, &sub_desc))
				{
					ROBOTICK_FATAL_EXIT("Invalid sub-field path after '%s' in: %s", field_token.c_str(), path);
				}

				ptr = static_cast<const uint8_t*>(sub_ptr);
				type = sub_type->id;
				size = sub_type->size;

				// consume any remaining chars (resolve_nested_member advanced the cursor via extract_next_token)
				while (*path_cursor)
					++path_cursor;
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
				return true;
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
				ROBOTICK_FATAL_EXIT("Type mismatch: %s vs %s (%s vs %s)",
					seed.source_field_path.c_str(),
					seed.dest_field_path.c_str(),
					src.type.get_debug_name(),
					dst.type.get_debug_name());

			if (src.size != dst.size)
				ROBOTICK_FATAL_EXIT(
					"Size mismatch: %s vs %s (%i vs %i)", seed.source_field_path.c_str(), seed.dest_field_path.c_str(), (int)src.size, (int)dst.size);

			if (has_connection_to_field(out_connections, dst.ptr))
				ROBOTICK_FATAL_EXIT("Duplicate connection to field: %s", seed.dest_field_path.c_str());

			out_connections[connection_index] =
				DataConnectionInfo{&seed, src.ptr, const_cast<void*>(dst.ptr), src.workload, dst.workload, src.size, src.type};

			connection_index++;
		}
	}

	void DataConnectionUtils::apply_struct_field_values(
		void* struct_ptr, const TypeDescriptor& struct_type_desc, const ArrayView<const FieldConfigEntry>& field_config_entries)
	{
		if (!struct_ptr)
			ROBOTICK_FATAL_EXIT("Struct-ptr not provided");

		const StructDescriptor* struct_desc = struct_type_desc.get_struct_desc();
		if (!struct_desc)
			ROBOTICK_FATAL_EXIT("Struct with no struct desc");

		(void)struct_desc; // not directly used now, but kept for parity with earlier checks

		for (const FieldConfigEntry& field_config_entry : field_config_entries)
		{
			const char* dotted = field_config_entry.first.c_str();
			const StringView& value = field_config_entry.second;

			void* target_ptr = nullptr;
			const TypeDescriptor* target_type = nullptr;
			const FieldDescriptor* target_field = nullptr;

			if (!resolve_nested_member(struct_ptr, &struct_type_desc, dotted, &target_ptr, &target_type, &target_field))
			{
				ROBOTICK_WARNING("Unable to find field '%s'", dotted);
				continue;
			}

			if (!target_type)
				ROBOTICK_FATAL_EXIT("Type resolution failed for field '%s'", dotted);

			if (!target_type->from_string(value.c_str(), target_ptr))
			{
				ROBOTICK_WARNING(
					"Unable to parse value-string '%s' for field: %s", value.c_str(), target_field ? target_field->name.c_str() : dotted);
			}
		}
	}

	std::tuple<void*, size_t, const FieldDescriptor*> DataConnectionUtils::find_field_info(const Engine& engine, const char* path)
	{
		const WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer();
		const Map<const char*, WorkloadInstanceInfo*>& instances = engine.get_all_instance_info_map();
		const char* path_cursor = path;

		// workload.section.field[.subfield...]
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

		// optional subfield chain
		if (*path_cursor != '\0')
		{
			void* sub_ptr = base_ptr;
			const TypeDescriptor* sub_type = field_type_desc;
			const FieldDescriptor* sub_desc = nullptr;

			if (!resolve_nested_member(sub_ptr, sub_type, path_cursor, &sub_ptr, &sub_type, &sub_desc))
			{
				ROBOTICK_FATAL_EXIT("Invalid sub-field path after '%s' in: %s", field_token.c_str(), path);
			}

			base_ptr = sub_ptr;
			size = sub_type->size;
			field = sub_desc;

			// consume any remaining chars
			while (*path_cursor)
				++path_cursor;
		}

		return {base_ptr, size, field};
	}

} // namespace robotick
