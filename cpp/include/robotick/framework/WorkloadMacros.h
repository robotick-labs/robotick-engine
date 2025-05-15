#pragma once

#include "robotick/framework/WorkloadRegistry.h"
#include "robotick/framework/utils_typename.h"
#include <cstddef>
#include <string>
#include <type_traits>
#include <map>

namespace robotick
{
    struct EmptyConfig
    {
    };

    struct EmptyInputs
    {
    };

    struct EmptyOutputs
    {
    };

    // === Detection traits ===

#define ROBOTICK_DECLARE_HAS_METHOD_TRAIT(trait_name, method_name)          \
    template <typename T>                                                   \
    class trait_name                                                        \
    {                                                                       \
    private:                                                                \
        template <typename U>                                               \
        static auto test(int)                                               \
            -> decltype(std::declval<U>().method_name(), std::true_type()); \
        template <typename>                                                 \
        static std::false_type test(...);                                   \
                                                                            \
    public:                                                                 \
        static constexpr bool value = decltype(test<T>(0))::value;          \
    };

    ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_pre_load, pre_load)
    ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_load, load)
    ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_setup, setup)
    ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_start, start)
    ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_stop, stop)
    ROBOTICK_DECLARE_HAS_METHOD_TRAIT(has_get_tick_rate_hz, get_tick_rate_hz)

    // === Internal registry ===

    inline std::map<std::string, const WorkloadRegistryEntry *> &get_mutable_registry()
    {
        static std::map<std::string, const WorkloadRegistryEntry *> registry;
        return registry;
    }

    inline void register_workload_entry(const WorkloadRegistryEntry &entry)
    {
        get_mutable_registry()[entry.name] = &entry;
    }

    inline const WorkloadRegistryEntry *get_workload_registry_entry(const std::string &name)
    {
        auto &map = get_mutable_registry();
        auto it = map.find(name);
        return it != map.end() ? it->second : nullptr;
    }

    // === Main template registration function ===

    template <typename Type, typename ConfigType, typename InputType, typename OutputType>
    const WorkloadRegistryEntry &register_workload()
    {
        const auto pre_load_fn = [](void *ptr)
        {
            if constexpr (has_pre_load<Type>::value)
                static_cast<Type *>(ptr)->pre_load();
        };

        const auto load_fn = [](void *ptr)
        {
            if constexpr (has_load<Type>::value)
                static_cast<Type *>(ptr)->load();
        };

        const auto setup_fn = [](void *ptr)
        {
            if constexpr (has_setup<Type>::value)
                static_cast<Type *>(ptr)->setup();
        };

        const auto start_fn = [](void *ptr)
        {
            if constexpr (has_start<Type>::value)
                static_cast<Type *>(ptr)->start();
        };

        const auto stop_fn = [](void *ptr)
        {
            if constexpr (has_stop<Type>::value)
                static_cast<Type *>(ptr)->stop();
        };

        const auto get_tick_rate_fn = [](void *ptr) -> double
        {
            if constexpr (has_get_tick_rate_hz<Type>::value)
                return static_cast<Type *>(ptr)->get_tick_rate_hz();
            else
                return 0.0;
        };

        static const WorkloadRegistryEntry entry = {
            get_clean_typename(typeid(Type)),
            sizeof(Type),
            alignof(Type),
            [](void *ptr)
            { new (ptr) Type(); },
            ConfigType::get_struct_reflection(),
            offsetof(Type, config),
            [](void *ptr, double dt)
            { static_cast<Type *>(ptr)->tick(dt); },
            pre_load_fn,
            load_fn,
            setup_fn,
            start_fn,
            stop_fn,
            get_tick_rate_fn};

        return entry;
    }

    // === Static self-registrar ===

    template <typename T>
    struct WorkloadRegistration
    {
        WorkloadRegistration(const WorkloadRegistryEntry &entry)
        {
            register_workload_entry(entry);
        }
    };

    // === Clean macro for usage in .cpp ===

#define ROBOTICK_REGISTER_WORKLOAD(Type, Config, Input, Output)       \
    static const ::robotick::WorkloadRegistryEntry &Type##_entry =    \
        ::robotick::register_workload<Type, Config, Input, Output>(); \
    static ::robotick::WorkloadRegistration<Type> Type##_registrar(Type##_entry)

} // namespace robotick
