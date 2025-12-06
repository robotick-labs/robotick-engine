// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"

#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringView.h"
#include "robotick/framework/utility/Pair.h"

namespace robotick
{
	struct WorkloadSeed
	{
		WorkloadSeed() = default;

		WorkloadSeed(const char* type_name, const char* unique_name)
			: type_id(type_name)
			, unique_name(unique_name)
		{
		}

		WorkloadSeed(const TypeId& type_id,
			const StringView& unique_name,
			float tick_rate_hz,
			const ArrayView<const WorkloadSeed*>& children = {},
			const ArrayView<const FieldConfigEntry>& config = {},
			const ArrayView<const FieldConfigEntry>& inputs = {})
			: type_id(type_id)
			, unique_name(unique_name)
			, tick_rate_hz(tick_rate_hz)
			, children(children)
			, config(config)
			, inputs(inputs)
		{
		}

		// Public data access
		TypeId type_id;
		StringView unique_name = nullptr;

		float tick_rate_hz = 0.0f;

		ArrayView<const WorkloadSeed*> children;

		ArrayView<const FieldConfigEntry> config;
		ArrayView<const FieldConfigEntry> inputs;
	};
} // namespace robotick
