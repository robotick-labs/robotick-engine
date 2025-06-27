// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"

#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/Pair.h"
#include "robotick/framework/common/StringView.h"

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#endif // #ifdef ROBOTICK_ENABLE_MODEL_HEAP

namespace robotick
{
	using ConfigEntry = Pair<FixedString64, FixedString64>;

	struct WorkloadSeed_v2
	{
		WorkloadSeed_v2() = default;

		WorkloadSeed_v2(const WorkloadRegistryEntry* type, const char* name) : type(type), name(name)
		{
		}

		// Public data access
		const WorkloadRegistryEntry* type = nullptr;
		StringView name = nullptr;

		float tick_rate_hz = 0.0f;

		ArrayView<const WorkloadSeed_v2*> children;

		ArrayView<ConfigEntry> config;
		ArrayView<ConfigEntry> inputs;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP

		// Dynamic Setters (for use on platforms where we can afford the heap-usage)
		WorkloadSeed_v2& set_tick_rate_hz(float rate);

		template <size_t N> WorkloadSeed_v2& set_children(const WorkloadSeed_v2* const (&in_children)[N]);
		template <size_t N> WorkloadSeed_v2& set_config(const ConfigEntry (&in_config)[N]);
		template <size_t N> WorkloadSeed_v2& set_inputs(const ConfigEntry (&in_inputs)[N]);

	  private:
		FixedString64 type_storage;
		FixedString64 name_storage;

		HeapVector<const WorkloadSeed_v2*> children_storage;
		HeapVector<ConfigEntry> config_storage;
		HeapVector<ConfigEntry> inputs_storage;

#endif // #ifdef ROBOTICK_ENABLE_MODEL_HEAP
	};

	template <size_t N> WorkloadSeed_v2& WorkloadSeed_v2::set_children(const WorkloadSeed_v2* const (&in_children)[N])
	{
		if (children_storage.size() > 0)
			ROBOTICK_FATAL_EXIT("set_children() may only be called once");

		children_storage.initialize(N);
		for (size_t i = 0; i < N; ++i)
			children_storage.data()[i] = in_children[i];

		children.use(children_storage);

		return *this;
	}

	template <size_t N> WorkloadSeed_v2& WorkloadSeed_v2::set_config(const ConfigEntry (&in_config)[N])
	{
		if (config_storage.size() > 0)
			ROBOTICK_FATAL_EXIT("set_config() may only be called once");

		config_storage.initialize(N);
		for (size_t i = 0; i < N; ++i)
			config_storage.data()[i] = in_config[i];

		config.use(config_storage);

		return *this;
	}

	template <size_t N> WorkloadSeed_v2& WorkloadSeed_v2::set_inputs(const ConfigEntry (&in_inputs)[N])
	{
		if (inputs_storage.size() > 0)
			ROBOTICK_FATAL_EXIT("set_inputs() may only be called once");

		inputs_storage.initialize(N);
		for (size_t i = 0; i < N; ++i)
			inputs_storage.data()[i] = in_inputs[i];

		inputs.use(inputs_storage);

		return *this;
	}

} // namespace robotick
