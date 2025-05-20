// Copyright 2025 Robotick Labs
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// WorkloadRegistry.h
#pragma once

#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/Typename.h"

#include <cstddef>
#include <map>
#include <string>
#include <type_traits>

namespace robotick
{
	struct WorkloadInstanceInfo;

	template <typename T> constexpr bool is_void_v = std::is_same_v<T, void>;

#define ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(trait_name, method_name, signature)                                                               \
	template <typename T> class trait_name                                                                                                           \
	{                                                                                                                                                \
	  private:                                                                                                                                       \
		template <typename U> static auto test(int) -> std::is_same<decltype(&U::method_name), signature>;                                           \
		template <typename> static std::false_type test(...);                                                                                        \
                                                                                                                                                     \
	  public:                                                                                                                                        \
		static constexpr bool value = decltype(test<T>(0))::value;                                                                                   \
	};

	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_set_children, set_children, void (T::*)(const std::vector<const WorkloadInstanceInfo*>&))
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_pre_load, pre_load, void (T::*)())
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_load, load, void (T::*)())
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_setup, setup, void (T::*)())
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_start, start, void (T::*)(double))
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_stop, stop, void (T::*)())
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_tick, tick, void (T::*)(double))

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

		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>& children);
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
		static WorkloadRegistry& get()
		{
			static WorkloadRegistry instance;
			return instance;
		}

		const WorkloadRegistryEntry* find(const std::string& name) const;
		void register_entry(const WorkloadRegistryEntry& entry);

	  private:
		WorkloadRegistry() = default;
		static std::map<std::string, const WorkloadRegistryEntry*>& registry();
	};

	template <typename Type, typename ConfigType = void, typename InputType = void, typename OutputType = void>
	const WorkloadRegistryEntry& register_workload()
	{
		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>& children) = nullptr;
		if constexpr (has_set_children<Type>::value)
			set_children_fn = +[](void* instance_raw_ptr, const std::vector<const WorkloadInstanceInfo*>& children)
			{
				static_cast<Type*>(instance_raw_ptr)->set_children(children);
			};

		void (*pre_load_fn)(void*) = nullptr;
		if constexpr (has_pre_load<Type>::value)
			pre_load_fn = +[](void* instance_raw_ptr)
			{
				static_cast<Type*>(instance_raw_ptr)->pre_load();
			};

		void (*load_fn)(void*) = nullptr;
		if constexpr (has_load<Type>::value)
			load_fn = +[](void* instance_raw_ptr)
			{
				static_cast<Type*>(instance_raw_ptr)->load();
			};

		void (*setup_fn)(void*) = nullptr;
		if constexpr (has_setup<Type>::value)
			setup_fn = +[](void* instance_raw_ptr)
			{
				static_cast<Type*>(instance_raw_ptr)->setup();
			};

		void (*start_fn)(void*, double) = nullptr;
		if constexpr (has_start<Type>::value)
			start_fn = +[](void* instance_raw_ptr, double tick_rate_hz)
			{
				static_cast<Type*>(instance_raw_ptr)->start(tick_rate_hz);
			};

		void (*stop_fn)(void*) = nullptr;
		if constexpr (has_stop<Type>::value)
			stop_fn = +[](void* instance_raw_ptr)
			{
				static_cast<Type*>(instance_raw_ptr)->stop();
			};

		void (*tick_fn)(void*, double) = nullptr;
		if constexpr (has_tick<Type>::value)
			tick_fn = +[](void* instance_raw_ptr, double time_delta)
			{
				static_cast<Type*>(instance_raw_ptr)->tick(time_delta);
			};

		const StructRegistryEntry* cfg_struct = nullptr;
		size_t cfg_offset = 0;
		if constexpr (!is_void_v<ConfigType>)
		{
			cfg_struct = ConfigType::get_struct_reflection();
			cfg_offset = offsetof(Type, config);
		}

		const StructRegistryEntry* in_struct = nullptr;
		size_t in_offset = 0;
		if constexpr (!is_void_v<InputType>)
		{
			in_struct = InputType::get_struct_reflection();
			in_offset = offsetof(Type, inputs);
		}

		const StructRegistryEntry* out_struct = nullptr;
		size_t out_offset = 0;
		if constexpr (!is_void_v<OutputType>)
		{
			out_struct = OutputType::get_struct_reflection();
			out_offset = offsetof(Type, outputs);
		}

		static const WorkloadRegistryEntry entry = {get_clean_typename(typeid(Type)), sizeof(Type), alignof(Type),
			[](void* instance_raw_ptr)
			{
				new (instance_raw_ptr) Type();
			},
			[](void* instance_raw_ptr)
			{
				static_cast<Type*>(instance_raw_ptr)->~Type();
			},
			cfg_struct, cfg_offset, in_struct, in_offset, out_struct, out_offset, set_children_fn, pre_load_fn, load_fn, setup_fn, start_fn, tick_fn,
			stop_fn};

		WorkloadRegistry::get().register_entry(entry);
		return entry;
	}

	template <typename T> struct WorkloadRegistration
	{
		WorkloadRegistration(const WorkloadRegistryEntry& e) { WorkloadRegistry::get().register_entry(e); }
	};

	template <typename T, typename Config = void, typename Inputs = void, typename Outputs = void> struct WorkloadAutoRegister
	{
		WorkloadAutoRegister()
		{
			static_assert(std::is_standard_layout<T>::value, "Workloads must be standard layout");
			static const auto& e = register_workload<T, Config, Inputs, Outputs>();
			static WorkloadRegistration<T> reg{e};
		}
	};

} // namespace robotick