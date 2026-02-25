// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/strings/StringView.h"

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
	};
} // namespace robotick
