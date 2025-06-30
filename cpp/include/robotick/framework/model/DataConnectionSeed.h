// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/common/StringView.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/FixedString.h"
#endif

namespace robotick
{
	struct DataConnectionSeed
	{
		DataConnectionSeed() = default;

		DataConnectionSeed(const char* source_field_path, const char* dest_field_path)
			: source_field_path(source_field_path)
			, dest_field_path(dest_field_path)
		{
		}

		StringView source_field_path = nullptr;
		StringView dest_field_path = nullptr;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		void set_source_field_path(const char* in_source)
		{
			source_field_path_storage = in_source;
			source_field_path = source_field_path_storage.c_str();
		}

		void set_dest_field_path(const char* in_dest)
		{
			dest_field_path_storage = in_dest;
			dest_field_path = dest_field_path_storage.c_str();
		}

	  private:
		FixedString64 source_field_path_storage;
		FixedString64 dest_field_path_storage;
#endif
	};
} // namespace robotick
