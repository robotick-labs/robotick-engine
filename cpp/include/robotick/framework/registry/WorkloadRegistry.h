#pragma once

#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/Typename.h"

#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>

namespace robotick
{

	template <typename T> constexpr bool is_void_v = std::is_same_v<T, void>;

// Method‐detection traits
#define ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(trait_name, method_name, signature)                                 \
	template <typename T> class trait_name                                                                             \
	{                                                                                                                  \
	  private:                                                                                                         \
		template <typename U> static auto test(int) -> std::is_same<decltype(&U::method_name), signature>;             \
		template <typename> static std::false_type test(...);                                                          \
                                                                                                                       \
	  public:                                                                                                          \
		static constexpr bool value = decltype(test<T>(0))::value;                                                     \
	};

	// no args:
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_pre_load, pre_load, void (T::*)());
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_load, load, void (T::*)());
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_setup, setup, void (T::*)());
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_start, start, void (T::*)());
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_stop, stop, void (T::*)());
	// single arg:
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT_WITH_SIG(has_tick, tick, void (T::*)(double));

	struct WorkloadRegistryEntry
	{
		std::string name;
		size_t size, alignment;
		void (*construct)(void *);

		const StructRegistryEntry *config_struct;
		size_t config_offset;

		const StructRegistryEntry *input_struct;
		size_t input_offset;

		const StructRegistryEntry *output_struct;
		size_t output_offset;

		void (*tick_fn)(void *, double);
		void (*pre_load_fn)(void *);
		void (*load_fn)(void *);
		void (*setup_fn)(void *);
		void (*start_fn)(void *);
		void (*stop_fn)(void *);
	};

	const WorkloadRegistryEntry *get_workload_registry_entry(const std::string &);
	void register_workload_entry(const WorkloadRegistryEntry &);

	// === the new, fixed register_workload ===
	template <typename Type, typename ConfigType = void, typename InputType = void, typename OutputType = void>
	const WorkloadRegistryEntry &register_workload()
	{
		// 1) Prepare lambdas for each optional method:
		void (*pre_load_fn)(void *) = nullptr;
		if constexpr (has_pre_load<Type>::value)
		{
			pre_load_fn = +[](void *p) { static_cast<Type *>(p)->pre_load(); };
		}

		void (*load_fn)(void *) = nullptr;
		if constexpr (has_load<Type>::value)
		{
			load_fn = +[](void *p) { static_cast<Type *>(p)->load(); };
		}

		void (*setup_fn)(void *) = nullptr;
		if constexpr (has_setup<Type>::value)
		{
			setup_fn = +[](void *p) { static_cast<Type *>(p)->setup(); };
		}

		void (*start_fn)(void *) = nullptr;
		if constexpr (has_start<Type>::value)
		{
			start_fn = +[](void *p) { static_cast<Type *>(p)->start(); };
		}

		void (*stop_fn)(void *) = nullptr;
		if constexpr (has_stop<Type>::value)
		{
			stop_fn = +[](void *p) { static_cast<Type *>(p)->stop(); };
		}

		void (*tick_fn)(void *, double) = nullptr;
		if constexpr (has_tick<Type>::value)
		{
			tick_fn = +[](void *p, double dt) { static_cast<Type *>(p)->tick(dt); };
		}

		// 2) Compute struct‐reflection pointers and offsets:
		const StructRegistryEntry *cfg_struct = nullptr;
		size_t cfg_offset = std::numeric_limits<size_t>::max();
		if constexpr (!is_void_v<ConfigType>)
		{
			cfg_struct = ConfigType::get_struct_reflection();
			cfg_offset = offsetof(Type, config);
		}

		const StructRegistryEntry *in_struct = nullptr;
		size_t in_offset = std::numeric_limits<size_t>::max();
		if constexpr (!is_void_v<InputType>)
		{
			in_struct = InputType::get_struct_reflection();
			in_offset = offsetof(Type, inputs);
		}

		const StructRegistryEntry *out_struct = nullptr;
		size_t out_offset = std::numeric_limits<size_t>::max();
		if constexpr (!is_void_v<OutputType>)
		{
			out_struct = OutputType::get_struct_reflection();
			out_offset = offsetof(Type, outputs);
		}

		// 3) Build the registry entry once:
		static const WorkloadRegistryEntry entry = {/* name      = */ get_clean_typename(typeid(Type)),
													/* size      = */ sizeof(Type),
													/* alignment = */ alignof(Type),
													/* construct = */ [](void *p) { new (p) Type(); },

													/* config_struct      = */ cfg_struct,
													/* config_offset      = */ cfg_offset,

													/* input_struct       = */ in_struct,
													/* input_offset       = */ in_offset,

													/* output_struct      = */ out_struct,
													/* output_offset      = */ out_offset,

													/* tick               = */ tick_fn,
													/* pre_load           = */ pre_load_fn,
													/* load               = */ load_fn,
													/* setup              = */ setup_fn,
													/* start              = */ start_fn,
													/* stop               = */ stop_fn};

		return entry;
	}

	// === Registration helper unchanged ===
	template <typename T> struct WorkloadRegistration
	{
		WorkloadRegistration(const WorkloadRegistryEntry &e)
		{
			register_workload_entry(e);
		}
	};

	// === WorkloadAutoRegister helper ===
	template <typename T, typename Config = void, typename Inputs = void, typename Outputs = void>
	struct WorkloadAutoRegister
	{
		WorkloadAutoRegister()
		{
			// no more standard-layout assert
			static const auto &e = register_workload<T, Config, Inputs, Outputs>();
			static WorkloadRegistration<T> reg{e};
		}
	};

} // namespace robotick
