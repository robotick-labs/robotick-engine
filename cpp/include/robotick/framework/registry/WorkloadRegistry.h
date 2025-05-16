#pragma once

#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/Typename.h"

#include <any>
#include <cstddef>
#include <map>
#include <string>
#include <type_traits>

namespace robotick
{
	// === Core registry entry ===

	struct WorkloadRegistryEntry
	{
		std::string name;
		size_t size;
		size_t alignment;

		void (*construct)(void *ptr);

		const StructRegistryEntry *config_struct = nullptr;
		size_t config_struct_offset = 0;

		void (*tick)(void *ptr, double time_delta) = nullptr;

		void (*pre_load)(void *ptr) = nullptr;
		void (*load)(void *ptr) = nullptr;
		void (*setup)(void *ptr) = nullptr;
		void (*start)(void *ptr) = nullptr;
		void (*stop)(void *ptr) = nullptr;

		double (*get_tick_rate_fn)(void *ptr) = nullptr;
	};

	// === API for registry lookup and population ===

	const WorkloadRegistryEntry *get_workload_registry_entry(const std::string &name);
	void register_workload_entry(const WorkloadRegistryEntry &entry);

	// === Empty default types ===

	struct EmptyConfig
	{
	};
	struct EmptyInputs
	{
	};
	struct EmptyOutputs
	{
	};

	// === Method detection traits ===

#define ROBOTICK_DECLARE_HAS_METHOD_TRAIT(trait_name, method_name)                                                     \
	template <typename T> class trait_name                                                                             \
	{                                                                                                                  \
	  private:                                                                                                         \
		template <typename U> static auto test(int) -> decltype(std::declval<U>().method_name(), std::true_type());    \
		template <typename> static std::false_type test(...);                                                          \
                                                                                                                       \
	  public:                                                                                                          \
		static constexpr bool value = decltype(test<T>(0))::value;                                                     \
	};

	ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_pre_load, pre_load)
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_load, load)
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_setup, setup)
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_start, start)
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_stop, stop)
	ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_get_tick_rate_hz, get_tick_rate_hz)

	// === Template-based registration ===

	template <typename Type, typename ConfigType, typename InputType, typename OutputType>
	const WorkloadRegistryEntry &register_workload()
	{
		const auto pre_load_fn = [](void *ptr) {
			if constexpr (has_pre_load<Type>::value)
				static_cast<Type *>(ptr)->pre_load();
		};

		const auto load_fn = [](void *ptr) {
			if constexpr (has_load<Type>::value)
				static_cast<Type *>(ptr)->load();
		};

		const auto setup_fn = [](void *ptr) {
			if constexpr (has_setup<Type>::value)
				static_cast<Type *>(ptr)->setup();
		};

		const auto start_fn = [](void *ptr) {
			if constexpr (has_start<Type>::value)
				static_cast<Type *>(ptr)->start();
		};

		const auto stop_fn = [](void *ptr) {
			if constexpr (has_stop<Type>::value)
				static_cast<Type *>(ptr)->stop();
		};

		const auto get_tick_rate_fn = [](void *ptr) -> double {
			if constexpr (has_get_tick_rate_hz<Type>::value)
				return static_cast<Type *>(ptr)->get_tick_rate_hz();
			else
				return 0.0;
		};

		const std::string type_name = get_clean_typename(typeid(Type));

		static const WorkloadRegistryEntry entry = {type_name,
													sizeof(Type),
													alignof(Type),
													[](void *ptr) { new (ptr) Type(); },
													ConfigType::get_struct_reflection(),
													offsetof(Type, config),
													[](void *ptr, double dt) { static_cast<Type *>(ptr)->tick(dt); },
													pre_load_fn,
													load_fn,
													setup_fn,
													start_fn,
													stop_fn,
													get_tick_rate_fn};

		return entry;
	}

	// === Registration helper ===

	template <typename T> struct WorkloadRegistration
	{
		WorkloadRegistration(const WorkloadRegistryEntry &entry)
		{
			register_workload_entry(entry);
		}
	};

	// === Optional: support manual type-key registration

	template <typename T> const WorkloadRegistryEntry *register_workload_type(const WorkloadRegistryEntry &entry)
	{
		register_workload_entry(entry);
		return &entry;
	}

#define ROBOTICK_REGISTER_WORKLOAD(Type, Config, Input, Output)                                                        \
	static const ::robotick::WorkloadRegistryEntry &Type##_entry =                                                     \
		::robotick::register_workload<Type, Config, Input, Output>();                                                  \
	static ::robotick::WorkloadRegistration<Type> Type##_registrar(Type##_entry)

} // namespace robotick
