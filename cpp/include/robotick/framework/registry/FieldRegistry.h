#pragma once

#include "robotick/framework/api.h"
#include <string>
#include <vector>
#include <typeindex>
#include <unordered_map>

namespace robotick
{

    struct FieldInfo
    {
        std::string name;
        size_t offset;
        std::type_index type;
    };

    struct StructRegistryEntry
    {
        std::string name;
        size_t size;
        std::vector<FieldInfo> fields;
    };

    ROBOTICK_API const StructRegistryEntry *register_struct(const std::string &name, size_t size, std::vector<FieldInfo> fields);
    ROBOTICK_API const StructRegistryEntry *get_struct(const std::string &name);

} // namespace robotick
