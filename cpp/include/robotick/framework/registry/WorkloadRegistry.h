// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace robotick
{
	// Forward declaration(s)
	class Engine;
	struct DataConnectionInfo;
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
	struct has_set_children<T, std::void_t<decltype(std::declval<T>().set_children(std::declval<const std::vector<const WorkloadInstanceInfo*>&>(),
								   std::declval<std::vector<DataConnectionInfo*>&>()))>> : std::true_type
	{
	};

	template <typename, typename = std::void_t<>> struct has_set_engine : std::false_type
	{
	};
	template <typename T>
	struct has_set_engine<T, std::void_t<decltype(std::declval<T>().set_engine(std::declval<const Engine&>()))>> : std::true_type
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
		FixedString64 name;
		TypeId type_id;
		size_t size;
		size_t alignment;
		void (*construct)(void*);
		void (*destruct)(void*);

		const StructRegistryEntry* config_struct;
		const StructRegistryEntry* input_struct;
		const StructRegistryEntry* output_struct;

		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>&, std::vector<DataConnectionInfo*>&);
		void (*set_engine_fn)(void*, const Engine&);
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
		const WorkloadRegistryEntry* find(const char* name) const;
		void register_entry(const WorkloadRegistryEntry& entry);

	  private:
		mutable std::mutex mutex;
		std::unordered_map<FixedString64, std::unique_ptr<WorkloadRegistryEntry>> entries;
	};

	// ——————————————————————————————————————————————————————————————————
	// 5) Core templates (definitions in header) …
	template <typename Type, typename ConfigType = void, typename InputType = void, typename OutputType = void>
	void register_workload(const char* workload_name, const char* config_name, const char* input_name, const char* output_name)
	{
		// Function pointers...
		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>&, std::vector<DataConnectionInfo*>&) = nullptr;
		void (*set_engine_fn)(void*, const Engine&) = nullptr;
		void (*pre_load_fn)(void*) = nullptr;
		void (*load_fn)(void*) = nullptr;
		void (*setup_fn)(void*) = nullptr;
		void (*start_fn)(void*, double) = nullptr;
		void (*tick_fn)(void*, double) = nullptr;
		void (*stop_fn)(void*) = nullptr;

		if constexpr (has_set_children<Type>::value)
			set_children_fn = +[](void* i, const std::vector<const WorkloadInstanceInfo*>& children, std::vector<DataConnectionInfo*>& pending)
			{
				static_cast<Type*>(i)->set_children(children, pending);
			};
		if constexpr (has_set_engine<Type>::value)
			set_engine_fn = +[](void* i, const Engine& e)
			{
				static_cast<Type*>(i)->set_engine(e);
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
			tick_fn = +[](void* i, double dt)
			{
				static_cast<Type*>(i)->tick(dt);
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

		static_assert((is_void_v<ConfigType> && !has_member_config<Type>::value) || (!is_void_v<ConfigType> && has_member_config<Type>::value),
			"Inconsistent config: type vs member presence mismatch on Workload registration");

		static_assert((is_void_v<InputType> && !has_member_inputs<Type>::value) || (!is_void_v<InputType> && has_member_inputs<Type>::value),
			"Inconsistent inputs: type vs member presence mismatch on Workload registration");

		static_assert((is_void_v<OutputType> && !has_member_outputs<Type>::value) || (!is_void_v<OutputType> && has_member_outputs<Type>::value),
			"Inconsistent outputs: type vs member presence mismatch on Workload registration");

		if constexpr (!is_void_v<ConfigType>)
			cfg_struct = FieldRegistry::get().register_struct(config_name, sizeof(ConfigType), TypeId{config_name}, offsetof(Type, config), {});
		if constexpr (!is_void_v<InputType>)
			in_struct = FieldRegistry::get().register_struct(input_name, sizeof(InputType), TypeId{input_name}, offsetof(Type, inputs), {});
		if constexpr (!is_void_v<OutputType>)
			out_struct = FieldRegistry::get().register_struct(output_name, sizeof(OutputType), TypeId{output_name}, offsetof(Type, outputs), {});

		const WorkloadRegistryEntry entry = {workload_name, TypeId{workload_name}, sizeof(Type), alignof(Type),
			[](void* ptr)
			{
				new (ptr) Type();
			},
			[](void* ptr)
			{
				static_cast<Type*>(ptr)->~Type();
			},
			cfg_struct, in_struct, out_struct, set_children_fn, set_engine_fn, pre_load_fn, load_fn, setup_fn, start_fn, tick_fn, stop_fn};

		WorkloadRegistry::get().register_entry(entry);
	}

	template <typename T, typename Config, typename Inputs, typename Outputs> struct WorkloadAutoRegister
	{
		explicit WorkloadAutoRegister(const char* workload_name, const char* config_name, const char* input_name, const char* output_name)
		{
			static_assert(std::is_standard_layout<T>::value, "Workloads must be standard layout");
			register_workload<T, Config, Inputs, Outputs>(workload_name, config_name, input_name, output_name);
		}
	};

#define ROBOTICK_DEFINE_WORKLOAD_1(Type) ROBOTICK_DEFINE_WORKLOAD_4(Type, void, void, void)

#define ROBOTICK_DEFINE_WORKLOAD_2(Type, Config) ROBOTICK_DEFINE_WORKLOAD_4(Type, Config, void, void)

#define ROBOTICK_DEFINE_WORKLOAD_3(Type, Config, Inputs) ROBOTICK_DEFINE_WORKLOAD_4(Type, Config, Inputs, void)

#define ROBOTICK_DEFINE_WORKLOAD_4(Type, Config, Inputs, Outputs)                                                                                    \
	__attribute__((used)) robotick::WorkloadAutoRegister<Type, Config, Inputs, Outputs> s_auto_register_##Type{#Type, #Config, #Inputs, #Outputs};   \
	__attribute__((used)) bool g_##Type##_NoDeadStrip = false;

#define GET_WORKLOAD_DEFINE_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define ROBOTICK_DEFINE_WORKLOAD(...)                                                                                                                \
	GET_WORKLOAD_DEFINE_MACRO(                                                                                                                       \
		__VA_ARGS__, ROBOTICK_DEFINE_WORKLOAD_4, ROBOTICK_DEFINE_WORKLOAD_3, ROBOTICK_DEFINE_WORKLOAD_2, ROBOTICK_DEFINE_WORKLOAD_1)(__VA_ARGS__)

#define ROBOTICK_KEEP_WORKLOAD(Type)                                                                                                                 \
	extern bool g_##Type##_NoDeadStrip;                                                                                                              \
	g_##Type##_NoDeadStrip = true;

} // namespace robotick
