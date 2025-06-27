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
	struct DataConnectionSeed_v2
	{
		StringView source_field_path = nullptr;
		StringView dest_field_path = nullptr;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		DataConnectionSeed_v2& set_source_field_path(const char* in_source);
		DataConnectionSeed_v2& set_dest_field_path(const char* in_dest);

	  private:
		FixedString64 source_field_path_storage;
		FixedString64 dest_field_path_storage;
#endif
	};
} // namespace robotick
