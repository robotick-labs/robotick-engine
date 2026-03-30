// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"

#include "robotick/api_base.h"
#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeMacros.h"

#include <climits>

namespace robotick
{
	const StructDescriptor* Blackboard::resolve_descriptor(const void* instance)
	{
		const Blackboard* blackboard = static_cast<const Blackboard*>(instance);
		return blackboard ? &(blackboard->get_struct_descriptor()) : nullptr;
	}

	bool Blackboard::plan_storage(const void* instance, DynamicStructStoragePlan& out_plan)
	{
		const Blackboard* blackboard = static_cast<const Blackboard*>(instance);
		if (!blackboard)
		{
			return false;
		}

		out_plan.size_bytes = blackboard->get_info().total_datablock_size;
		out_plan.alignment = blackboard->compute_storage_alignment();
		return true;
	}

	bool Blackboard::bind_storage(
		void* instance, const WorkloadsBuffer& workloads_buffer, size_t storage_offset_in_workloads_buffer, size_t storage_size_bytes)
	{
		Blackboard* blackboard = static_cast<Blackboard*>(instance);
		if (!blackboard)
		{
			return false;
		}

		size_t storage_cursor = storage_offset_in_workloads_buffer;
		blackboard->bind(workloads_buffer, storage_cursor);

		const size_t expected_end = storage_offset_in_workloads_buffer + storage_size_bytes;
		ROBOTICK_ASSERT_MSG(storage_cursor == expected_end,
			"Blackboard::bind_storage() consumed %zu bytes, expected %zu bytes",
			storage_cursor - storage_offset_in_workloads_buffer,
			storage_size_bytes);
		return true;
	}

	ROBOTICK_REGISTER_DYNAMIC_STRUCT(Blackboard, Blackboard::resolve_descriptor, Blackboard::plan_storage, Blackboard::bind_storage)

	void Blackboard::initialize_fields(const HeapVector<FieldDescriptor>& fields)
	{
		info.struct_descriptor.fields.use(const_cast<FieldDescriptor*>(fields.data()), fields.size());
		compute_total_datablock_size();
	}

	void Blackboard::initialize_fields(const ArrayView<FieldDescriptor>& fields)
	{
		info.struct_descriptor.fields = fields;
		compute_total_datablock_size();
	}

	static bool safe_add(size_t lhs, size_t rhs, size_t& out)
	{
		constexpr size_t max_size = SIZE_MAX;
		if (rhs > max_size - lhs)
			return false;
		out = lhs + rhs;
		return true;
	}

	static bool safe_align(size_t value, size_t alignment, size_t& out)
	{
		if (alignment == 0)
			return false;
		size_t remainder = value % alignment;
		size_t aligned = value;
		if (remainder != 0)
		{
			size_t delta = alignment - remainder;
			if (!safe_add(aligned, delta, aligned))
				return false;
		}
		out = aligned;
		return true;
	}

	static size_t compute_and_apply_layout(const size_t blackboard_offset_in_workloads_buffer,
		FieldDescriptor* fields,
		const size_t field_count,
		const size_t start_offset_in_workloads_buffer,
		const bool write_offsets)
	{
		size_t current_offset_in_workloads_buffer = start_offset_in_workloads_buffer;

		for (size_t i = 0; i < field_count; ++i)
		{
			FieldDescriptor& field = fields[i];
			const TypeDescriptor* type = field.find_type_descriptor();
			ROBOTICK_ASSERT_MSG(type != nullptr, "Field has no type descriptor");

			size_t aligned_offset = 0;
			if (!safe_align(current_offset_in_workloads_buffer, type->alignment, aligned_offset))
				ROBOTICK_FATAL_EXIT("Field '%s' uses invalid alignment (%zu) for type '%s'", field.name.c_str(), type->alignment, type->name.c_str());
			current_offset_in_workloads_buffer = aligned_offset;

			if (write_offsets)
			{
				if (current_offset_in_workloads_buffer < blackboard_offset_in_workloads_buffer)
				{
					ROBOTICK_FATAL_EXIT("Field '%s' offset underflows (current=%zu < blackboard=%zu)",
						field.name.c_str(),
						current_offset_in_workloads_buffer,
						blackboard_offset_in_workloads_buffer);
				}
				field.offset_within_container = current_offset_in_workloads_buffer - blackboard_offset_in_workloads_buffer;
			}

			if (type->size == 0)
				ROBOTICK_FATAL_EXIT("Field '%s' references zero-sized type '%s'", field.name.c_str(), type->name.c_str());

			size_t next_offset = 0;
			if (!safe_add(current_offset_in_workloads_buffer, type->size, next_offset))
				ROBOTICK_FATAL_EXIT("Field '%s' layout overflow for type '%s'", field.name.c_str(), type->name.c_str());
			current_offset_in_workloads_buffer = next_offset;
		}

		return current_offset_in_workloads_buffer;
	}

	void Blackboard::compute_total_datablock_size()
	{
		const size_t start_offset = 0;
		const bool write_offsets = false;

		info.total_datablock_size =
			compute_and_apply_layout(0, info.struct_descriptor.fields.data_ptr(), info.struct_descriptor.fields.size(), start_offset, write_offsets);
	}

	size_t Blackboard::compute_storage_alignment() const
	{
		size_t max_alignment = 1;
		for (const FieldDescriptor& field : info.struct_descriptor.fields)
		{
			const TypeDescriptor* type = field.find_type_descriptor();
			ROBOTICK_ASSERT_MSG(type != nullptr, "Blackboard field '%s' has no type descriptor", field.name.c_str());
			ROBOTICK_ASSERT_MSG(type->alignment != 0, "Blackboard field '%s' uses invalid alignment (0)", field.name.c_str());
			if (type->alignment > max_alignment)
			{
				max_alignment = type->alignment;
			}
		}
		return max_alignment;
	}

	void Blackboard::bind(const WorkloadsBuffer& workloads_buffer, size_t& datablock_offset_in_workloads_buffer)
	{
		const size_t blackboard_offset_in_workloads_buffer = (uint8_t*)this - workloads_buffer.raw_ptr();

		const size_t start_offset_in_workloads_buffer = datablock_offset_in_workloads_buffer;
		const bool write_offsets = true;

		datablock_offset_in_workloads_buffer = compute_and_apply_layout(blackboard_offset_in_workloads_buffer,
			info.struct_descriptor.fields.data_ptr(),
			info.struct_descriptor.fields.size(),
			start_offset_in_workloads_buffer,
			write_offsets);
	}

	const FieldDescriptor* Blackboard::find_field(const char* field_name) const
	{
		return info.find_field(field_name);
	}

	void* Blackboard::find_field_data(const char* field_name, const FieldDescriptor*& found_field) const
	{
		found_field = find_field(field_name);
		if (found_field)
		{
			void* field_data = found_field->get_data_ptr((void*)this);
			return field_data;
		}

		return nullptr;
	}

	bool Blackboard::has(const char* field_name) const
	{
		return (find_field(field_name) != nullptr);
	}

	bool Blackboard::set(const char* field_name, void* value, const size_t size)
	{
		const FieldDescriptor* found_field = find_field(field_name);
		if (found_field)
		{
			return set(*found_field, value, size);
		}
		return false;
	}

	void* Blackboard::get(const char* field_name, const size_t size) const
	{
		const FieldDescriptor* found_field = find_field(field_name);
		if (found_field)
		{
			return get(*found_field, size);
		}
		return nullptr;
	}

	bool Blackboard::set(const FieldDescriptor& field, void* value, size_t size)
	{
		void* field_data = field.get_data_ptr((void*)this);
		if (field_data)
		{
			ROBOTICK_ASSERT(size == field.find_type_descriptor()->size);
			memcpy(field_data, value, size);
			return true;
		}
		return false;
	}

	void* Blackboard::get(const FieldDescriptor& field, size_t size) const
	{
		void* field_data = field.get_data_ptr((void*)this);
		if (field_data)
		{
			ROBOTICK_ASSERT(size == field.find_type_descriptor()->size);
			return field_data;
		}
		return nullptr;
	}

} // namespace robotick
