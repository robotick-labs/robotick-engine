// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeDescriptor.h"

#include "robotick/api_base.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/memory/StdApproved.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/strings/StringUtils.h"

#include <cinttypes>
#include <cstring>
#include <limits>

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

	namespace
	{
		struct EnumStorageValue
		{
			uint64_t raw_unsigned = 0;
			int64_t raw_signed = 0;
			bool has_signed_value = false;
		};

		static bool read_enum_storage(const void* input, const EnumDescriptor& desc, EnumStorageValue& out_value)
		{
			if (!input)
				return false;

			out_value.has_signed_value = desc.is_signed;
			switch (desc.underlying_size)
			{
			case 1:
				if (desc.is_signed)
				{
					const int8_t v = *reinterpret_cast<const int8_t*>(input);
					out_value.raw_signed = v;
					out_value.raw_unsigned = static_cast<uint8_t>(v);
				}
				else
				{
					const uint8_t v = *reinterpret_cast<const uint8_t*>(input);
					out_value.raw_unsigned = v;
				}
				return true;
			case 2:
				if (desc.is_signed)
				{
					const int16_t v = *reinterpret_cast<const int16_t*>(input);
					out_value.raw_signed = v;
					out_value.raw_unsigned = static_cast<uint16_t>(v);
				}
				else
				{
					const uint16_t v = *reinterpret_cast<const uint16_t*>(input);
					out_value.raw_unsigned = v;
				}
				return true;
			case 4:
				if (desc.is_signed)
				{
					const int32_t v = *reinterpret_cast<const int32_t*>(input);
					out_value.raw_signed = v;
					out_value.raw_unsigned = static_cast<uint32_t>(v);
				}
				else
				{
					const uint32_t v = *reinterpret_cast<const uint32_t*>(input);
					out_value.raw_unsigned = v;
				}
				return true;
			case 8:
				if (desc.is_signed)
				{
					const int64_t v = *reinterpret_cast<const int64_t*>(input);
					out_value.raw_signed = v;
					out_value.raw_unsigned = static_cast<uint64_t>(v);
				}
				else
				{
					const uint64_t v = *reinterpret_cast<const uint64_t*>(input);
					out_value.raw_unsigned = v;
				}
				return true;
			default:
				return false;
			}
		}

		template <typename T> static bool check_signed_range(int64_t value)
		{
			return value >= static_cast<int64_t>(std_approved::numeric_limits<T>::min()) &&
				   value <= static_cast<int64_t>(std_approved::numeric_limits<T>::max());
		}

		template <typename T> static bool check_unsigned_range(uint64_t value)
		{
			return value <= static_cast<uint64_t>(std_approved::numeric_limits<T>::max());
		}

		static bool write_signed_value(int64_t value, void* output, const EnumDescriptor& desc)
		{
			switch (desc.underlying_size)
			{
			case 1:
				if (!check_signed_range<int8_t>(value))
					return false;
				*reinterpret_cast<int8_t*>(output) = static_cast<int8_t>(value);
				return true;
			case 2:
				if (!check_signed_range<int16_t>(value))
					return false;
				*reinterpret_cast<int16_t*>(output) = static_cast<int16_t>(value);
				return true;
			case 4:
				if (!check_signed_range<int32_t>(value))
					return false;
				*reinterpret_cast<int32_t*>(output) = static_cast<int32_t>(value);
				return true;
			case 8:
				*reinterpret_cast<int64_t*>(output) = static_cast<int64_t>(value);
				return true;
			default:
				return false;
			}
		}

		static bool write_unsigned_value(uint64_t value, void* output, const EnumDescriptor& desc)
		{
			switch (desc.underlying_size)
			{
			case 1:
				if (!check_unsigned_range<uint8_t>(value))
					return false;
				*reinterpret_cast<uint8_t*>(output) = static_cast<uint8_t>(value);
				return true;
			case 2:
				if (!check_unsigned_range<uint16_t>(value))
					return false;
				*reinterpret_cast<uint16_t*>(output) = static_cast<uint16_t>(value);
				return true;
			case 4:
				if (!check_unsigned_range<uint32_t>(value))
					return false;
				*reinterpret_cast<uint32_t*>(output) = static_cast<uint32_t>(value);
				return true;
			case 8:
				*reinterpret_cast<uint64_t*>(output) = static_cast<uint64_t>(value);
				return true;
			default:
				return false;
			}
		}

		static bool convert_raw_to_signed(uint64_t raw_value, const EnumDescriptor& desc, int64_t& out)
		{
			switch (desc.underlying_size)
			{
			case 1:
				out = static_cast<int8_t>(static_cast<uint8_t>(raw_value));
				return true;
			case 2:
				out = static_cast<int16_t>(static_cast<uint16_t>(raw_value));
				return true;
			case 4:
				out = static_cast<int32_t>(static_cast<uint32_t>(raw_value));
				return true;
			case 8:
				out = static_cast<int64_t>(raw_value);
				return true;
			default:
				return false;
			}
		}

		static const EnumValue* find_enum_value_by_name(const EnumDescriptor& desc, const char* name)
		{
			for (const EnumValue& entry : desc.values)
			{
				if (string_equals(entry.name.c_str(), name))
				{
					return &entry;
				}
			}
			return nullptr;
		}

		static const EnumValue* find_enum_value_by_raw(const EnumDescriptor& desc, uint64_t raw_value)
		{
			for (const EnumValue& entry : desc.values)
			{
				if (entry.value == raw_value)
				{
					return &entry;
				}
			}
			return nullptr;
		}
	} // namespace

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
		if (name == "int32_t")
		{
			return ::sscanf(input, "%" SCNd32, reinterpret_cast<int32_t*>(out_value)) == 1;
		}
		if (name == "uint16_t")
		{
			return ::sscanf(input, "%hu", reinterpret_cast<uint16_t*>(out_value)) == 1;
		}
		if (name == "uint32_t")
		{
			return ::sscanf(input, "%" SCNu32, reinterpret_cast<uint32_t*>(out_value)) == 1;
		}
		const EnumDescriptor* enum_desc = get_enum_desc();
		if (enum_desc)
		{
			const EnumValue* named_value = find_enum_value_by_name(*enum_desc, input);
			if (named_value)
			{
				if (enum_desc->is_signed)
				{
					int64_t signed_value = 0;
					if (!convert_raw_to_signed(named_value->value, *enum_desc, signed_value))
						return false;
					return write_signed_value(signed_value, out_value, *enum_desc);
				}
				return write_unsigned_value(named_value->value, out_value, *enum_desc);
			}

			if (enum_desc->is_signed)
			{
				long long parsed = 0;
				if (::sscanf(input, "%lld", &parsed) != 1)
				{
					return false;
				}
				return write_signed_value(parsed, out_value, *enum_desc);
			}
			else
			{
				unsigned long long parsed = 0;
				if (::sscanf(input, "%llu", &parsed) != 1)
				{
					return false;
				}
				return write_unsigned_value(parsed, out_value, *enum_desc);
			}
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

		if (name == "int32_t")
		{
			const int32_t v = *reinterpret_cast<const int32_t*>(value);
			const int written = ::snprintf(output_buffer, output_buffer_size, "%d", static_cast<int>(v));
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

		const EnumDescriptor* enum_desc = get_enum_desc();
		if (enum_desc)
		{
			EnumStorageValue raw_value{};
			if (!read_enum_storage(value, *enum_desc, raw_value))
				return false;

			const EnumValue* entry = find_enum_value_by_raw(*enum_desc, raw_value.raw_unsigned);

			if (entry)
			{
				const size_t len = entry->name.length();
				if (len + 1 > output_buffer_size)
					return false;

				::memcpy(output_buffer, entry->name.c_str(), len);
				output_buffer[len] = '\0';
				return true;
			}

			if (enum_desc->is_signed && raw_value.has_signed_value)
			{
				const int written = ::snprintf(output_buffer, output_buffer_size, "%lld", static_cast<long long>(raw_value.raw_signed));
				if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
				{
					::memset(output_buffer, 0, output_buffer_size);
					return false;
				}
				return true;
			}
			else
			{
				const int written = ::snprintf(output_buffer, output_buffer_size, "%llu", static_cast<unsigned long long>(raw_value.raw_unsigned));
				if (written < 0 || static_cast<size_t>(written) >= output_buffer_size)
				{
					::memset(output_buffer, 0, output_buffer_size);
					return false;
				}
				return true;
			}
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
