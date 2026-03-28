// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/strings/StringView.h"

#include <cstdint>

namespace robotick
{
	struct TelemetryPeerSeed
	{
		StringView model_name;
		StringView host;
		uint16_t telemetry_port = 0;
		bool is_gateway = false;
	};
} // namespace robotick
