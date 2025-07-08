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
		// TODO - remove the const-cast - should work fine as const (StructDescriptor may need some adjusting)
		info.struct_descriptor.fields.use(const_cast<FieldDescriptor*>(fields.data()), fields.size());
	}

	void Blackboard::initialize_fields(const ArrayView<FieldDescriptor>& fields)
	{
		info.struct_descriptor.fields = fields;
	}

	const FieldDescriptor* Blackboard::find_field(const char* field_name) const
	{
		for (const auto& field : info.struct_descriptor.fields)
		{
			if (field.name == field_name)
			{
				return &field;
			}
		}

		return nullptr;
	}

	void* Blackboard::find_field_data(const char* field_name, const FieldDescriptor*& found_field) const
	{
		found_field = find_field(field_name);
		if (found_field)
		{
			const size_t datablock_offset = get_datablock_offset();
			ROBOTICK_ASSERT(datablock_offset != OFFSET_UNBOUND);

			uint8_t* datablock_ptr = (uint8_t*)this + datablock_offset;

			void* field_data = found_field->get_data_ptr(datablock_ptr);
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
		const FieldDescriptor* found_field = nullptr;
		void* field_data = find_field_data(field_name, found_field);
		if (field_data && found_field)
		{
			ROBOTICK_ASSERT(size == found_field->find_type_descriptor()->size);
			memcpy(field_data, &value, size);
			return true;
		}
		return false;
	}

	void* Blackboard::get(const char* field_name, const size_t size) const
	{
		const FieldDescriptor* found_field = nullptr;
		void* field_data = find_field_data(field_name, found_field);
		if (field_data && found_field)
		{
			ROBOTICK_ASSERT(size == found_field->find_type_descriptor()->size);
			return field_data;
		}
		return nullptr;
	}

} // namespace robotick