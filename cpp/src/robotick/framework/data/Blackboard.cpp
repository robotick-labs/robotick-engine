// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/registry/TypeMacros.h"

namespace robotick
{
	const StructDescriptor* Blackboard::resolve_descriptor(const void* instance)
	{
		const Blackboard* blackboard = static_cast<const Blackboard*>(instance);
		return blackboard ? &(blackboard->get_struct_descriptor()) : nullptr;
	}

	ROBOTICK_REGISTER_DYNAMIC_STRUCT(Blackboard, Blackboard::resolve_descriptor)

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

	inline size_t align_up(size_t value, size_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	static size_t compute_and_apply_layout(FieldDescriptor* fields, const size_t field_count, const size_t start_offset, const bool write_offsets)
	{
		size_t current_offset = start_offset;

		for (size_t i = 0; i < field_count; ++i)
		{
			FieldDescriptor& field = fields[i];
			const TypeDescriptor* type = field.find_type_descriptor();
			ROBOTICK_ASSERT_MSG(type != nullptr, "Field has no type descriptor");

			current_offset = align_up(current_offset, type->alignment);

			if (write_offsets)
			{
				field.offset_within_container = current_offset;
			}

			current_offset += type->size;
		}

		return current_offset;
	}

	void Blackboard::compute_total_datablock_size()
	{
		const size_t start_offset = 0;
		const bool write_offsets = false;

		info.total_datablock_size =
			compute_and_apply_layout(info.struct_descriptor.fields.data_ptr(), info.struct_descriptor.fields.size(), start_offset, write_offsets);
	}

	void Blackboard::bind(size_t& datablock_offset)
	{
		const size_t start_offset = datablock_offset;
		const bool write_offsets = true;

		datablock_offset =
			compute_and_apply_layout(info.struct_descriptor.fields.data_ptr(), info.struct_descriptor.fields.size(), start_offset, write_offsets);
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