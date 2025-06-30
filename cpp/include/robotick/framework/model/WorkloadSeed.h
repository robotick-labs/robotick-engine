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

	struct WorkloadSeed
	{
		WorkloadSeed() = default;

		WorkloadSeed(const char* type_name, const char* name)
			: type_id(type_name)
			, name(name)
		{
		}

		WorkloadSeed(const TypeId& type_id,
			const StringView& name,
			float tick_rate_hz,
			const ArrayView<const WorkloadSeed*>& children = {},
			const ArrayView<const ConfigEntry>& config = {},
			const ArrayView<const ConfigEntry>& inputs = {})
			: type_id(type_id)
			, name(name)
			, tick_rate_hz(tick_rate_hz)
			, children(children)
			, config(config)
			, inputs(inputs)
		{
		}

		// Public data access
		TypeId type_id;
		StringView name = nullptr;

		float tick_rate_hz = 0.0f;

		ArrayView<const WorkloadSeed*> children;

		ArrayView<const ConfigEntry> config;
		ArrayView<const ConfigEntry> inputs;

#ifdef ROBOTICK_ENABLE_MODEL_HEAP

		WorkloadSeed& set_type_name(const char* type_name)
		{
			if (!TypeRegistry::get().find_by_id(TypeId(type_name)))
				ROBOTICK_FATAL_EXIT("Unable to find type '%s' for workload", type_name);

			type_id = TypeId(type_name);
			return *this;
		}

		WorkloadSeed& set_name(const char* in_name)
		{
			if (!in_name || !*in_name)
				ROBOTICK_FATAL_EXIT("Null or empty name specified for workload-seed");

			name_storage = in_name;
			name = name_storage.c_str();

			return *this;
		}

		// Dynamic Setters (for use on platforms where we can afford the heap-usage)
		WorkloadSeed& set_tick_rate_hz(const float in_tick_rate_hz)
		{
			if (in_tick_rate_hz < 0)
				ROBOTICK_FATAL_EXIT("tick_rate_hz must be >= 0: %f", in_tick_rate_hz);

			tick_rate_hz = in_tick_rate_hz;

			return *this;
		};

		template <size_t N> WorkloadSeed& set_children(const WorkloadSeed* const (&in_children)[N]);
		template <size_t N> WorkloadSeed& set_config(const ConfigEntry (&in_config)[N]);
		template <size_t N> WorkloadSeed& set_inputs(const ConfigEntry (&in_inputs)[N]);

	  private:
		FixedString64 name_storage;

		HeapVector<const WorkloadSeed*> children_storage;
		HeapVector<ConfigEntry> config_storage;
		HeapVector<ConfigEntry> inputs_storage;

#endif // #ifdef ROBOTICK_ENABLE_MODEL_HEAP
	};

#ifdef ROBOTICK_ENABLE_MODEL_HEAP

	template <size_t N> WorkloadSeed& WorkloadSeed::set_children(const WorkloadSeed* const (&in_children)[N])
	{
		if (children_storage.size() > 0)
			ROBOTICK_FATAL_EXIT("set_children() may only be called once");

		children_storage.initialize(N);
		for (size_t i = 0; i < N; ++i)
			children_storage.data()[i] = in_children[i];

		children.use(children_storage);

		return *this;
	}

	template <size_t N> WorkloadSeed& WorkloadSeed::set_config(const ConfigEntry (&in_config)[N])
	{
		if (config_storage.size() > 0)
			ROBOTICK_FATAL_EXIT("set_config() may only be called once");

		config_storage.initialize(N);
		for (size_t i = 0; i < N; ++i)
			config_storage.data()[i] = in_config[i];

		config.use(config_storage.data(), config_storage.size());

		return *this;
	}

	template <size_t N> WorkloadSeed& WorkloadSeed::set_inputs(const ConfigEntry (&in_inputs)[N])
	{
		if (inputs_storage.size() > 0)
			ROBOTICK_FATAL_EXIT("set_inputs() may only be called once");

		inputs_storage.initialize(N);
		for (size_t i = 0; i < N; ++i)
			inputs_storage.data()[i] = in_inputs[i];

		inputs.use(inputs_storage.data(), inputs_storage.size());

		return *this;
	}

#endif // #ifdef ROBOTICK_ENABLE_MODEL_HEAP

} // namespace robotick
