// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstddef>
#include <type_traits>
#include <vector>

namespace robotick::registry
{

	// --- Method traits ---

	template <typename T, typename = void> struct has_set_children : std::false_type
	{
	};
	template <typename T>
	struct has_set_children<T,
		std::void_t<decltype(std::declval<T>().set_children(
			std::declval<const HeapVector<const WorkloadInstanceInfo*>&>(), std::declval<const HeapVector<DataConnectionInfo>&>()))>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_set_engine : std::false_type
	{
	};
	template <typename T>
	struct has_set_engine<T, std::void_t<decltype(std::declval<T>().set_engine(std::declval<const Engine&>()))>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_pre_load : std::false_type
	{
	};
	template <typename T> struct has_pre_load<T, std::void_t<decltype(std::declval<T>().pre_load())>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_load : std::false_type
	{
	};
	template <typename T> struct has_load<T, std::void_t<decltype(std::declval<T>().load())>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_setup : std::false_type
	{
	};
	template <typename T> struct has_setup<T, std::void_t<decltype(std::declval<T>().setup())>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_start : std::false_type
	{
	};
	template <typename T> struct has_start<T, std::void_t<decltype(std::declval<T>().start(std::declval<double>()))>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_tick : std::false_type
	{
	};
	template <typename T> struct has_tick<T, std::void_t<decltype(std::declval<T>().tick(std::declval<const TickInfo&>()))>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_stop : std::false_type
	{
	};
	template <typename T> struct has_stop<T, std::void_t<decltype(std::declval<T>().stop())>> : std::true_type
	{
	};

	// --- Member traits ---

	template <typename T, typename = void> struct has_member_config : std::false_type
	{
	};
	template <typename T> struct has_member_config<T, std::void_t<decltype(std::declval<T>().config)>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_member_inputs : std::false_type
	{
	};
	template <typename T> struct has_member_inputs<T, std::void_t<decltype(std::declval<T>().inputs)>> : std::true_type
	{
	};

	template <typename T, typename = void> struct has_member_outputs : std::false_type
	{
	};
	template <typename T> struct has_member_outputs<T, std::void_t<decltype(std::declval<T>().outputs)>> : std::true_type
	{
	};

	// --- Optional member type resolution ---

	template <typename T, bool Present = has_member_config<T>::value> struct config_type
	{
		using type = void;
	};
	template <typename T> struct config_type<T, true>
	{
		using type = std::remove_reference_t<decltype(std::declval<T>().config)>;
	};

	template <typename T, bool Present = has_member_inputs<T>::value> struct inputs_type
	{
		using type = void;
	};
	template <typename T> struct inputs_type<T, true>
	{
		using type = std::remove_reference_t<decltype(std::declval<T>().inputs)>;
	};

	template <typename T, bool Present = has_member_outputs<T>::value> struct outputs_type
	{
		using type = void;
	};
	template <typename T> struct outputs_type<T, true>
	{
		using type = std::remove_reference_t<decltype(std::declval<T>().outputs)>;
	};

	template <typename T> using config_t = typename config_type<T>::type;
	template <typename T> using inputs_t = typename inputs_type<T>::type;
	template <typename T> using outputs_t = typename outputs_type<T>::type;

	template <typename T> static void construct_fn(void* ptr)
	{
		new (ptr) T();
	}

	template <typename T> static void destruct_fn(void* ptr)
	{
		static_cast<T*>(ptr)->~T();
	}

	template <typename T>
	static void set_children_fn(void* self, const HeapVector<const WorkloadInstanceInfo*>& children, const HeapVector<DataConnectionInfo>& pending)
	{
		static_cast<T*>(self)->set_children(children, pending);
	}

	template <typename T> static void set_engine_fn(void* self, const Engine& e)
	{
		static_cast<T*>(self)->set_engine(e);
	}

	template <typename T> static void pre_load_fn(void* self)
	{
		static_cast<T*>(self)->pre_load();
	}

	template <typename T> static void load_fn(void* self)
	{
		static_cast<T*>(self)->load();
	}

	template <typename T> static void setup_fn(void* self)
	{
		static_cast<T*>(self)->setup();
	}

	template <typename T> static void start_fn(void* self, double t)
	{
		static_cast<T*>(self)->start(t);
	}

	template <typename T> static void tick_fn(void* self, const TickInfo& tick)
	{
		static_cast<T*>(self)->tick(tick);
	}

	template <typename T> static void stop_fn(void* self)
	{
		static_cast<T*>(self)->stop();
	}

	// Factory to build a constexpr WorkloadDescriptor
	template <typename T>
	const WorkloadDescriptor make_workload_descriptor(
		const TypeDescriptor* config_type, const TypeDescriptor* inputs_type, const TypeDescriptor* outputs_type)
	{
		WorkloadDescriptor desc{};

		if constexpr (has_member_config<T>::value)
		{
			desc.config_offset = offsetof(T, config);
			desc.config_desc = config_type;
			ROBOTICK_ASSERT(desc.config_desc);
		}
		else
		{
			desc.config_offset = SIZE_MAX;
		}

		if constexpr (has_member_inputs<T>::value)
		{
			desc.inputs_offset = offsetof(T, inputs);
			desc.inputs_desc = inputs_type;
			ROBOTICK_ASSERT(desc.inputs_desc);
		}
		else
		{
			desc.inputs_offset = SIZE_MAX;
		}

		if constexpr (has_member_outputs<T>::value)
		{
			desc.outputs_offset = offsetof(T, outputs);
			desc.outputs_desc = outputs_type;
			ROBOTICK_ASSERT(desc.outputs_desc);
		}
		else
		{
			desc.outputs_offset = SIZE_MAX;
		}

		// init function pointers:

		desc.construct_fn = &construct_fn<T>;
		desc.destruct_fn = &destruct_fn<T>;

		if constexpr (has_set_children<T>::value)
			desc.set_children_fn = &set_children_fn<T>;
		if constexpr (has_set_engine<T>::value)
			desc.set_engine_fn = &set_engine_fn<T>;
		if constexpr (has_pre_load<T>::value)
			desc.pre_load_fn = &pre_load_fn<T>;
		if constexpr (has_load<T>::value)
			desc.load_fn = &load_fn<T>;
		if constexpr (has_setup<T>::value)
			desc.setup_fn = &setup_fn<T>;
		if constexpr (has_start<T>::value)
			desc.start_fn = &start_fn<T>;
		if constexpr (has_tick<T>::value)
			desc.tick_fn = &tick_fn<T>;
		if constexpr (has_stop<T>::value)
			desc.stop_fn = &stop_fn<T>;

		return desc;
	}

} // namespace robotick::registry