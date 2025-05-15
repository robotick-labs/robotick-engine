#pragma once

#include "robotick/framework/FieldRegistry.h"
#include "robotick/framework/FixedString.h"
#include "robotick/framework/utils_typename.h"
#include <any>
#include <map>
#include <string>
#include <cstring>
#include <stdexcept>
#include <type_traits>

template <typename T>
T &any_cast_ref(std::any &val)
{
    if (!val.has_value())
        throw std::runtime_error("Missing config value");
    return std::any_cast<T &>(val);
}

namespace robotick
{
    template <typename T>
    struct type_identity
    {
        using type = T;
    };

    template <typename Func>
    inline bool dispatch_fixed_string(const std::type_index &type, Func &&fn)
    {
        if (type == typeid(FixedString8))
            return fn(type_identity<FixedString8>{});
        if (type == typeid(FixedString16))
            return fn(type_identity<FixedString16>{});
        if (type == typeid(FixedString32))
            return fn(type_identity<FixedString32>{});
        if (type == typeid(FixedString64))
            return fn(type_identity<FixedString64>{});
        if (type == typeid(FixedString128))
            return fn(type_identity<FixedString128>{});
        if (type == typeid(FixedString256))
            return fn(type_identity<FixedString256>{});
        if (type == typeid(FixedString512))
            return fn(type_identity<FixedString512>{});
        if (type == typeid(FixedString1024))
            return fn(type_identity<FixedString1024>{});
        return false;
    }

    inline void apply_struct_fields(void *struct_ptr, const StructRegistryEntry &info, const std::map<std::string, std::any> &config)
    {
        for (const auto &field : info.fields)
        {
            auto it = config.find(field.name);
            if (it == config.end())
                continue;

            void *field_ptr = static_cast<uint8_t *>(struct_ptr) + field.offset;

            if (field.type == typeid(std::string))
            {
                *static_cast<std::string *>(field_ptr) = std::any_cast<std::string>(it->second);
            }
            else if (field.type == typeid(double))
            {
                *static_cast<double *>(field_ptr) = std::any_cast<double>(it->second);
            }
            else if (field.type == typeid(int))
            {
                *static_cast<int *>(field_ptr) = std::any_cast<int>(it->second);
            }
            else if (dispatch_fixed_string(field.type, [&](auto T) -> bool
                                           {
            using FS = typename decltype(T)::type;
            if (it->second.type() == typeid(std::string)) {
                *static_cast<FS*>(field_ptr) = std::any_cast<std::string>(it->second).c_str();
            } else if (it->second.type() == typeid(const char*)) {
                *static_cast<FS*>(field_ptr) = std::any_cast<const char*>(it->second);
            } else {
                throw std::runtime_error("Invalid type for FixedString field: " + field.name);
            }
            return true; }))
            {
                continue;
            }
            else
            {
                throw std::runtime_error("Unsupported config type: " + get_clean_typename(field.type) + " for field: " + field.name);
            }
        }
    }

} // namespace robotick
