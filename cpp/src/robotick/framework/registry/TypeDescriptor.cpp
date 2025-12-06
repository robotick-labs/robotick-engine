// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeDescriptor.h"

#include "robotick/api_base.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/strings/StringUtils.h"

#include <cstring>

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

	static size_t limited_strlen(const char* str, size_t max_length)
	{
		size_t len = 0;
		while (len < max_length && str[len])
		{
			++len;
		}
		return len;
	}

	bool TypeDescriptor::from_string(const char* input, void* out_value) const
	{
		if (!input || !out_value)
		{
			return false;
		}

		if (name == "float")
		{
			return ::sscanf(input, "%f", reinterpret_cast<float*>(out_value)) == 1;
		}
		if (name == "double")
		{
			return ::sscanf(input, "%lf", reinterpret_cast<double*>(out_value)) == 1;
		}
		if (name == "bool")
		{
			if (string_equals(input, "1") || string_equals_ignore_case(input, "true"))
			{
				*reinterpret_cast<bool*>(out_value) = true;
			}
			else if (string_equals(input, "0") || string_equals_ignore_case(input, "false"))
			{
				*reinterpret_cast<bool*>(out_value) = false;
			}
			else
			{
				return false;
			}

			return true;
		}
		if (name == "int")
		{
			return ::sscanf(input, "%d", reinterpret_cast<int*>(out_value)) == 1;
		}
		if (name == "uint16_t")
		{
			return ::sscanf(input, "%hu", reinterpret_cast<uint16_t*>(out_value)) == 1;
		}
		if (name == "uint32_t")
		{
			unsigned long parsed = 0;
			const int read = ::sscanf(input, "%lu", &parsed);
			if (read == 1)
			{
				*reinterpret_cast<uint32_t*>(out_value) = static_cast<uint32_t>(parsed);
				return true;
			}
			return false;
		}
		if (mime_type == "text/plain")
		{
			if (size == 0)
				return false;

			char* dest = reinterpret_cast<char*>(out_value);
			const size_t max_copy = size - 1;
			const size_t actual_len = limited_strlen(input, max_copy);

			::memcpy(dest, input, actual_len);
			dest[actual_len] = '\0';
			for (size_t i = actual_len + 1; i < size; ++i)
				dest[i] = '\0';

			return true;
		}

		// fallback
		return false;
	}

	bool TypeDescriptor::to_string(const void* value, char* output_buffer, size_t output_buffer_size) const
	{
		if (!value || !output_buffer || output_buffer_size == 0)
		{
			ROBOTICK_FATAL_EXIT("Invalid arguments to to_string()");
			return false;
		}

		// Zero the output buffer before formatting
		::memset(output_buffer, 0, output_buffer_size);

		if (name == "float")
		{
			const float v = *reinterpret_cast<const float*>(value);
			const int written = ::snprintf(output_buffer, output_buffer_size, "%g", v);
			if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
			{
				::memset(output_buffer, 0, output_buffer_size);
				return false;
			}
			return true;
		}

		if (name == "double")
		{
			const double v = *reinterpret_cast<const double*>(value);
			const int written = ::snprintf(output_buffer, output_buffer_size, "%g", v);
			if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
			{
				::memset(output_buffer, 0, output_buffer_size);
				return false;
			}
			return true;
		}

		if (name == "bool")
		{
			const bool v = *reinterpret_cast<const bool*>(value);
			const char* s = v ? "true" : "false";
			const size_t len = ::strlen(s);

			if (len + 1 > output_buffer_size)
			{
				return false;
			}

			::memcpy(output_buffer, s, len + 1);
			return true;
		}

		if (name == "int")
		{
			const int v = *reinterpret_cast<const int*>(value);
			const int written = ::snprintf(output_buffer, output_buffer_size, "%d", v);
			if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
			{
				::memset(output_buffer, 0, output_buffer_size);
				return false;
			}
			return true;
		}

		if (name == "uint16_t")
		{
			const uint16_t v = *reinterpret_cast<const uint16_t*>(value);
			const int written = ::snprintf(output_buffer, output_buffer_size, "%hu", v);
			if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
			{
				::memset(output_buffer, 0, output_buffer_size);
				return false;
			}
			return true;
		}

		if (name == "uint32_t")
		{
			const uint32_t v = *reinterpret_cast<const uint32_t*>(value);
			const int written = ::snprintf(output_buffer, output_buffer_size, "%lu", static_cast<unsigned long>(v));
			if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
			{
				::memset(output_buffer, 0, output_buffer_size);
				return false;
			}
			return true;
		}

		if (mime_type == "text/plain")
		{
			if (size == 0 || output_buffer_size == 0)
				return false;

			const char* src = reinterpret_cast<const char*>(value);
			const size_t max_value_len = size - 1;
			const size_t len = limited_strlen(src, max_value_len);

			if (len + 1 > output_buffer_size)
			{
				return false;
			}

			::memcpy(output_buffer, src, len);
			output_buffer[len] = '\0';
			return true;
		}

		// fallback — unsupported
		return false;
	}

} // namespace robotick
