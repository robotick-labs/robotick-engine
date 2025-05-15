#pragma once

#include <string>
#include <typeindex>

inline std::string get_clean_typename(const std::type_index &t)
{
    std::string name = t.name();
    if (name.rfind("class ", 0) == 0)
        return name.substr(6);
    if (name.rfind("struct ", 0) == 0)
        return name.substr(7);
    return name;
}
