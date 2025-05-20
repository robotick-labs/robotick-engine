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

#pragma once

#include "robotick/framework/registry/FieldRegistry.h"
#include <typeinfo>

// clang-format off

#define ROBOTICK_OFFSET_OF(type, field) \
    reinterpret_cast<size_t>(           \
        &reinterpret_cast<const volatile char&>(((type*)0)->field))

#define ROBOTICK_DECLARE_FIELDS(...)                                       \
    static const ::robotick::StructRegistryEntry* get_struct_reflection(); \
    static std::vector<::robotick::FieldInfo>     get_fields();

#define ROBOTICK_DEFINE_FIELDS(Type, ...)                                \
	static_assert(std::is_standard_layout<Type>::value, "Workload field-containers must be standard layout"); \
    const ::robotick::StructRegistryEntry* Type::get_struct_reflection() \
    {                                                                    \
        static const auto* entry = ::robotick::register_struct(          \
            #Type, sizeof(Type), Type::get_fields());                    \
        return entry;                                                    \
    }                                                                    \
    std::vector<::robotick::FieldInfo> Type::get_fields()                \
    {                                                                    \
        return {__VA_ARGS__};                                            \
    }

#define ROBOTICK_FIELD(type, field)                                            \
    ::robotick::FieldInfo                                                      \
    {                                                                          \
        #field, ROBOTICK_OFFSET_OF(type, field), typeid(decltype(type::field)) \
    }
// clang-format on
