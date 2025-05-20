// Copyright 2025 Robotick Labs CIC
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
