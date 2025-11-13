// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeDescriptor.h"

#include "robotick/api_base.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeRegistry.h"

namespace robotick
{
	const TypeDescriptor s_type_desc_void{
		"void",
		GET_TYPE_ID(void),
		0,
		1,
		TypeCategory::Primitive,
		{},		// .workload_desc etc. unused for primitives
		nullptr // mime_type
	};

	void* FieldDescriptor::get_data_ptr(void* container_ptr) const
	{
		ROBOTICK_ASSERT_MSG(this->offset_within_container != OFFSET_UNBOUND,
			"FieldDescriptor::get_data_ptr() - offset_within_container has not yet been bound (field '%s' with type '%s')",
			this->name.c_str(),
			this->type_id.get_debug_name());
		uint8_t* data_ptr = (uint8_t*)container_ptr + this->offset_within_container;
		return data_ptr;
	}

	void* FieldDescriptor::get_data_ptr(
		WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const TypeDescriptor&, const size_t struct_offset) const
	{
		ROBOTICK_ASSERT(
			instance.offset_in_workloads_buffer != OFFSET_UNBOUND && "Workload object instance offset should have been correctly set by now");
		ROBOTICK_ASSERT(struct_offset != OFFSET_UNBOUND && "struct offset should have been correctly set by now");
		ROBOTICK_ASSERT(this->offset_within_container != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

		uint8_t* base_ptr = workloads_buffer.raw_ptr();
		uint8_t* instance_ptr = base_ptr + instance.offset_in_workloads_buffer;
		uint8_t* struct_ptr = instance_ptr + struct_offset;
		return get_data_ptr(struct_ptr);
	}

	const TypeDescriptor* FieldDescriptor::find_type_descriptor() const
	{
		const TypeDescriptor* field_type = TypeRegistry::get().find_by_id(type_id);
		ROBOTICK_ASSERT_MSG(field_type != nullptr,
			"Unable to find TypeDescriptor '%s' for field '%s' - this shouldn't be possible - perhaps they are being pruned by the linker?",
			this->type_id.get_debug_name(),
			this->name.c_str());
		return field_type;
	}

	const FieldDescriptor* StructDescriptor::find_field(const char* field_name) const
	{
		for (const FieldDescriptor& field : fields)
		{
			if (field.name == field_name)
			{
				return &field;
			}
		}

		return nullptr;
	}

	inline bool case_insensitive_equals(const char* a, const char* b)
	{
		if (!a || !b)
		{
			return false;
		}

		while (*a && *b)
		{
			if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
			{
				return false;
			}
			++a;
			++b;
		}

		return *a == *b;
	}

	bool TypeDescriptor::from_string(const char* input, void* out_value) const
	{
		if (!input || !out_value)
		{
			ROBOTICK_FATAL_EXIT("!");
			return false;
		}

		if (name == "float")
		{
			return std::sscanf(input, "%f", reinterpret_cast<float*>(out_value)) == 1;
		}
		if (name == "double")
		{
			return std::sscanf(input, "%lf", reinterpret_cast<double*>(out_value)) == 1;
		}
		if (name == "bool")
		{
			if ((std::strcmp(input, "1") == 0) || case_insensitive_equals(input, "true"))
			{
				*reinterpret_cast<bool*>(out_value) = true;
			}
			else if ((std::strcmp(input, "0") == 0) || case_insensitive_equals(input, "false"))
			{
				*reinterpret_cast<bool*>(out_value) = false;
			}
			else
			{
				ROBOTICK_FATAL_EXIT("Invalid boolean string: %s", input);
				return false;
			}

			return true;
		}
		if (name == "int")
		{
			return std::sscanf(input, "%d", reinterpret_cast<int*>(out_value)) == 1;
		}
		if (name == "uint16_t")
		{
			return std::sscanf(input, "%hu", reinterpret_cast<uint16_t*>(out_value)) == 1;
		}
		if (name == "uint32_t")
		{
			return std::sscanf(input, "%u", reinterpret_cast<uint32_t*>(out_value)) == 1;
		}
		if (mime_type == "text/plain")
		{
			std::strncpy(reinterpret_cast<char*>(out_value), input, size);
			return true;
		}

		// fallback
		ROBOTICK_FATAL_EXIT("!");
		return false;
	}

} // namespace robotick