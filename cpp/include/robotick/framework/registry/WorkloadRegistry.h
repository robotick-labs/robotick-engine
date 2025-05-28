// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/Typename.h"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace robotick
{

	// Forward declaration
	struct WorkloadInstanceInfo;

	// Utility to detect void
	template <typename T> constexpr bool is_void_v = std::is_same_v<T, void>;

	// ——————————————————————————————————————————————————————————————————
	// 1) Method-presence traits
	// ——————————————————————————————————————————————————————————————————

	template <typename, typename = std::void_t<>> struct has_set_children : std::false_type
	{
	};
	template <typename T>
	struct has_set_children<T, std::void_t<decltype(std::declval<T>().set_children(std::declval<const std::vector<const WorkloadInstanceInfo*>&>()))>>
		: std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_pre_load : std::false_type
	{
	};
	template <typename T> struct has_pre_load<T, std::void_t<decltype(std::declval<T>().pre_load())>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_load : std::false_type
	{
	};
	template <typename T> struct has_load<T, std::void_t<decltype(std::declval<T>().load())>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_setup : std::false_type
	{
	};
	template <typename T> struct has_setup<T, std::void_t<decltype(std::declval<T>().setup())>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_start : std::false_type
	{
	};
	template <typename T> struct has_start<T, std::void_t<decltype(std::declval<T>().start(std::declval<double>()))>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_tick : std::false_type
	{
	};
	template <typename T> struct has_tick<T, std::void_t<decltype(std::declval<T>().tick(std::declval<double>()))>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_stop : std::false_type
	{
	};
	template <typename T> struct has_stop<T, std::void_t<decltype(std::declval<T>().stop())>> : std::true_type
	{
	};

	// ——————————————————————————————————————————————————————————————————
	// 2) Member-presence traits
	// ——————————————————————————————————————————————————————————————————

	template <typename, typename = std::void_t<>> struct has_member_config : std::false_type
	{
	};
	template <typename T> struct has_member_config<T, std::void_t<decltype(std::declval<T>().config)>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_member_inputs : std::false_type
	{
	};
	template <typename T> struct has_member_inputs<T, std::void_t<decltype(std::declval<T>().inputs)>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_member_outputs : std::false_type
	{
	};
	template <typename T> struct has_member_outputs<T, std::void_t<decltype(std::declval<T>().outputs)>> : std::true_type
	{
	};

	// ——————————————————————————————————————————————————————————————————
	// 3) Member-or-void helpers
	// ——————————————————————————————————————————————————————————————————

	template <typename T, bool Has = has_member_config<T>::value> struct config_or_void
	{
		using type = void;
	};
	template <typename T> struct config_or_void<T, true>
	{
		using type = std::remove_reference_t<decltype(std::declval<T>().config)>;
	};

	template <typename T, bool Has = has_member_inputs<T>::value> struct inputs_or_void
	{
		using type = void;
	};
	template <typename T> struct inputs_or_void<T, true>
	{
		using type = std::remove_reference_t<decltype(std::declval<T>().inputs)>;
	};

	template <typename T, bool Has = has_member_outputs<T>::value> struct outputs_or_void
	{
		using type = void;
	};
	template <typename T> struct outputs_or_void<T, true>
	{
		using type = std::remove_reference_t<decltype(std::declval<T>().outputs)>;
	};

	// Aliases for macro use
	template <typename T> using config_t = typename config_or_void<T>::type;
	template <typename T> using inputs_t = typename inputs_or_void<T>::type;
	template <typename T> using outputs_t = typename outputs_or_void<T>::type;

	// ——————————————————————————————————————————————————————————————————
	// 4) WorkloadRegistryEntry & singleton interface
	// ——————————————————————————————————————————————————————————————————

	struct WorkloadRegistryEntry
	{
		std::string name;
		size_t size;
		size_t alignment;
		void (*construct)(void*);
		void (*destruct)(void*);

		const StructRegistryEntry* config_struct;
		size_t config_offset;

		const StructRegistryEntry* input_struct;
		size_t input_offset;

		const StructRegistryEntry* output_struct;
		size_t output_offset;

		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>&);
		void (*pre_load_fn)(void*);
		void (*load_fn)(void*);
		void (*setup_fn)(void*);
		void (*start_fn)(void*, double);
		void (*tick_fn)(void*, double);
		void (*stop_fn)(void*);
	};

	class WorkloadRegistry
	{
	  public:
		static WorkloadRegistry& get();
		const WorkloadRegistryEntry* find(const std::string& name) const;
		void register_entry(const WorkloadRegistryEntry& entry);

	  private:
		mutable std::mutex mutex;
		std::map<std::string, std::unique_ptr<WorkloadRegistryEntry>> entries;
	};

	// ——————————————————————————————————————————————————————————————————
	// 5) Core templates (definitions in header) …
	template <typename Type, typename ConfigType = void, typename InputType = void, typename OutputType = void> void register_workload()
	{
		// Pointers initialized to nullptr
		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>&) = nullptr;
		void (*pre_load_fn)(void*) = nullptr;
		void (*load_fn)(void*) = nullptr;
		void (*setup_fn)(void*) = nullptr;
		void (*start_fn)(void*, double) = nullptr;
		void (*tick_fn)(void*, double) = nullptr;
		void (*stop_fn)(void*) = nullptr;

		// Bind methods if present
		if constexpr (has_set_children<Type>::value)
			set_children_fn = +[](void* i, const std::vector<const WorkloadInstanceInfo*>& c)
			{
				static_cast<Type*>(i)->set_children(c);
			};
		if constexpr (has_pre_load<Type>::value)
			pre_load_fn = +[](void* i)
			{
				static_cast<Type*>(i)->pre_load();
			};
		if constexpr (has_load<Type>::value)
			load_fn = +[](void* i)
			{
				static_cast<Type*>(i)->load();
			};
		if constexpr (has_setup<Type>::value)
			setup_fn = +[](void* i)
			{
				static_cast<Type*>(i)->setup();
			};
		if constexpr (has_start<Type>::value)
			start_fn = +[](void* i, double d)
			{
				static_cast<Type*>(i)->start(d);
			};
		if constexpr (has_tick<Type>::value)
			tick_fn = +[](void* i, double time_delta)
			{
				static_cast<Type*>(i)->tick(time_delta);
			};
		if constexpr (has_stop<Type>::value)
			stop_fn = +[](void* i)
			{
				static_cast<Type*>(i)->stop();
			};

		// Collect struct metadata
		const StructRegistryEntry* cfg_struct = nullptr;
		const StructRegistryEntry* in_struct = nullptr;
		const StructRegistryEntry* out_struct = nullptr;
		size_t cfg_offset = 0, in_offset = 0, out_offset = 0;

		if constexpr (!is_void_v<ConfigType>)
		{
			cfg_struct = FieldRegistry::get().register_struct(get_clean_typename(typeid(ConfigType)), sizeof(ConfigType), {});
			cfg_offset = offsetof(Type, config);
		}
		if constexpr (!is_void_v<InputType>)
		{
			in_struct = FieldRegistry::get().register_struct(get_clean_typename(typeid(InputType)), sizeof(InputType), {});
			in_offset = offsetof(Type, inputs);
		}
		if constexpr (!is_void_v<OutputType>)
		{
			out_struct = FieldRegistry::get().register_struct(get_clean_typename(typeid(OutputType)), sizeof(OutputType), {});
			out_offset = offsetof(Type, outputs);
		}

		// Create/register the static entry
		const WorkloadRegistryEntry entry = {get_clean_typename(typeid(Type)), sizeof(Type), alignof(Type),
			[](void* ptr)
			{
				new (ptr) Type();
			},
			[](void* ptr)
			{
				static_cast<Type*>(ptr)->~Type();
			},
			cfg_struct, cfg_offset, in_struct, in_offset, out_struct, out_offset, set_children_fn, pre_load_fn, load_fn, setup_fn, start_fn, tick_fn,
			stop_fn};

		WorkloadRegistry::get().register_entry(entry);
	}

	template <typename T, typename Config = void, typename Inputs = void, typename Outputs = void> struct WorkloadAutoRegister
	{
		WorkloadAutoRegister()
		{
			static_assert(std::is_standard_layout<T>::value, "Workloads must be standard layout");
			register_workload<T, Config, Inputs, Outputs>();
		}
	};

	// ——————————————————————————————————————————————————————————————————
	// 6) One-line macro for optional fields
	// ——————————————————————————————————————————————————————————————————

#define ROBOTICK_DEFINE_WORKLOAD(Type)                                                                                                               \
	static robotick::WorkloadAutoRegister<Type, robotick::config_t<Type>, robotick::inputs_t<Type>, robotick::outputs_t<Type>> s_auto_register_##Type;

} // namespace robotick
