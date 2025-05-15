#pragma once

#include "robotick/framework/FieldRegistry.h"
#include <typeinfo>

#define ROBOTICK_OFFSET_OF(type, field) \
    reinterpret_cast<size_t>(&reinterpret_cast<const volatile char &>(((type *)0)->field))

#define ROBOTICK_DECLARE_FIELDS(...)                                       \
    static const ::robotick::StructRegistryEntry *get_struct_reflection(); \
    static std::vector<::robotick::FieldInfo> get_fields();                \
    static const char *get_struct_name();

#define ROBOTICK_DEFINE_FIELDS(Type, ...)                                                                \
    const ::robotick::StructRegistryEntry *Type::get_struct_reflection()                                 \
    {                                                                                                    \
        static const auto *entry = ::robotick::register_struct(#Type, sizeof(Type), Type::get_fields()); \
        return entry;                                                                                    \
    }                                                                                                    \
    const char *Type::get_struct_name() { return #Type; }                                                \
    std::vector<::robotick::FieldInfo> Type::get_fields()                                                \
    {                                                                                                    \
        return {__VA_ARGS__};                                                                            \
    }

#define ROBOTICK_FIELD(type, field) \
    ::robotick::FieldInfo { #field, ROBOTICK_OFFSET_OF(type, field), typeid(decltype(type::field)) }
