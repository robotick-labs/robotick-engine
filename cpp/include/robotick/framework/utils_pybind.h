#pragma once
#include <pybind11/pybind11.h>
#include "robotick/framework/FieldRegistry.h"

namespace py = pybind11;

namespace robotick
{

    void marshal_struct_to_dict(void *struct_ptr, const StructRegistryEntry &info, py::dict &py_out);
    void marshal_dict_to_struct(const py::dict &py_in, const StructRegistryEntry &info, void *struct_ptr);

}
