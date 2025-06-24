// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/utils/TypeId.h"

#include <stddef.h>
#include <stdint.h>

namespace robotick
{
	struct TypeDescriptor
	{
		const char* name; // e.g. "int"
		TypeId id;		  // Unique ID per type
		size_t size;	  // Size in bytes

		// Converts data to string form, writing to buffer. Null-terminated.
		// Returns true if successful.
		bool (*to_string)(const void* data, char* out_buffer, size_t buffer_size);

		// Parses string and stores result in out_data.
		bool (*from_string)(const char* str, void* out_data);

		TypeDescriptor* next_entry = nullptr; // required by ForwardLinkedList
	};
} // namespace robotick
