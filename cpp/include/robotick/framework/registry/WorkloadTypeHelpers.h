// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/TypeTraits.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstddef>

namespace robotick::registry
{

	// --- Method traits ---

	template <typename T, typename = void> struct has_set_children : FalseType<>
	{
	};
	template <typename T>
	struct has_set_children<T,
		void_t<decltype(declval<T>().set_children(declval<const HeapVector<const WorkloadInstanceInfo*>&>(),
			declval<HeapVector<DataConnectionInfo>&>()))>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_set_engine : FalseType<>
	{
	};
	template <typename T>
	struct has_set_engine<T, void_t<decltype(declval<T>().set_engine(declval<const Engine&>()))>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_pre_load : FalseType<>
	{
	};
	template <typename T> struct has_pre_load<T, void_t<decltype(declval<T>().pre_load())>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_load : FalseType<>
	{
	};
	template <typename T> struct has_load<T, void_t<decltype(declval<T>().load())>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_setup : FalseType<>
	{
	};
	template <typename T> struct has_setup<T, void_t<decltype(declval<T>().setup())>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_start : FalseType<>
	{
	};
	template <typename T> struct has_start<T, void_t<decltype(declval<T>().start(declval<float>()))>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_tick : FalseType<>
	{
	};
	template <typename T> struct has_tick<T, void_t<decltype(declval<T>().tick(declval<const TickInfo&>()))>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_stop : FalseType<>
	{
	};
	template <typename T> struct has_stop<T, void_t<decltype(declval<T>().stop())>> : TrueType<>
	{
	};

	// --- Member traits ---

	template <typename T, typename = void> struct has_member_config : FalseType<>
	{
	};
	template <typename T> struct has_member_config<T, void_t<decltype(declval<T>().config)>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_member_inputs : FalseType<>
	{
	};
	template <typename T> struct has_member_inputs<T, void_t<decltype(declval<T>().inputs)>> : TrueType<>
	{
	};

	template <typename T, typename = void> struct has_member_outputs : FalseType<>
	{
	};
	template <typename T> struct has_member_outputs<T, void_t<decltype(declval<T>().outputs)>> : TrueType<>
	{
	};

	// --- Optional member type resolution ---

	template <typename T, bool Present = has_member_config<T>::value> struct config_type
	{
		using type = void;
	};
	template <typename T> struct config_type<T, true>
	{
		using type = remove_reference_t<decltype(declval<T>().config)>;
	};

	template <typename T, bool Present = has_member_inputs<T>::value> struct inputs_type
	{
		using type = void;
	};
	template <typename T> struct inputs_type<T, true>
	{
		using type = remove_reference_t<decltype(declval<T>().inputs)>;
	};

	template <typename T, bool Present = has_member_outputs<T>::value> struct outputs_type
	{
		using type = void;
	};
	template <typename T> struct outputs_type<T, true>
	{
		using type = remove_reference_t<decltype(declval<T>().outputs)>;
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
	static void set_children_fn(void* self, const HeapVector<const WorkloadInstanceInfo*>& children, HeapVector<DataConnectionInfo>& pending)
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

	template <typename T> static void start_fn(void* self, float t)
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

		// if we have a member of each of config/inputs/outputs, AND we've been provided with a
		// corresponding type (e.g. since it's not an empty struct) then register it:

		if constexpr (has_member_config<T>::value)
		{
			if (config_type)
			{
				desc.config_offset = offsetof(T, config);
				desc.config_desc = config_type;
			}
		}

		if constexpr (has_member_inputs<T>::value)
		{
			if (inputs_type)
			{
				desc.inputs_offset = offsetof(T, inputs);
				desc.inputs_desc = inputs_type;
			}
		}

		if constexpr (has_member_outputs<T>::value)
		{
			if (outputs_type)
			{
				desc.outputs_offset = offsetof(T, outputs);
				desc.outputs_desc = outputs_type;
			}
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
