#pragma once

#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/FieldUtils.h"
#include "robotick/framework/FixedString.h"
#include "robotick/framework/utils_pybind.h"
#include "robotick/framework/WorkloadMacros.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <mutex>
#include <iostream>

namespace py = pybind11;
using namespace robotick;

// === Structs ===

struct PythonConfig
{
    FixedString128 script_name;
    FixedString64 class_name;
    double tick_rate = 30.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(PythonConfig,
                       ROBOTICK_FIELD(PythonConfig, script_name),
                       ROBOTICK_FIELD(PythonConfig, class_name),
                       ROBOTICK_FIELD(PythonConfig, tick_rate))

struct PythonInputs
{
    double example_in = 0.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(PythonInputs,
                       ROBOTICK_FIELD(PythonInputs, example_in))

struct PythonOutputs
{
    double example_out = 0.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(PythonOutputs,
                       ROBOTICK_FIELD(PythonOutputs, example_out))

// === Workload ===

class PythonWorkload
{
public:
    PythonConfig config;
    PythonInputs inputs;
    PythonOutputs outputs;

public:
    void load()
    {
        // Initialize interpreter once (with thread support), then drop GIL
        static std::once_flag init_flag;
        std::call_once(init_flag, []()
                       {
                           py::initialize_interpreter(/*start_embedded*/);
                           PyEval_SaveThread(); // release the GIL
                       });

        // Acquire GIL for imports and object creation
        py::gil_scoped_acquire gil;

        // Build configuration dict
        py::dict py_cfg;
        marshal_struct_to_dict(&config, *config.get_struct_reflection(), py_cfg);

        // Import module, get class, and instantiate Python object
        py_module = py::module_::import(config.script_name.c_str());
        py_class = py_module.attr(config.class_name.c_str());
        py_instance = py_class(py_cfg);

        std::cout << "[Python] Loaded "
                  << config.script_name.c_str()
                  << "." << config.class_name.c_str() << std::endl;
    }

    void tick(double dt)
    {
        if (!py_instance)
            return;

        // Acquire GIL for calling into Python
        py::gil_scoped_acquire gil;

        // Prepare inputs
        py::dict py_in;
        marshal_struct_to_dict(&inputs, *inputs.get_struct_reflection(), py_in);
        py::dict py_out;

        try
        {
            // Call the Python 'tick' method
            py_instance.attr("tick")(dt, py_in, py_out);
            // Marshal outputs back into C++ struct
            marshal_dict_to_struct(py_out, *outputs.get_struct_reflection(), &outputs);
        }
        catch (const py::error_already_set &e)
        {
            std::cerr << "[Python ERROR] " << e.what() << std::endl;
        }
    }

    double get_tick_rate_hz() const
    {
        return config.tick_rate;
    }

private:
    // Python embedding members
    py::object py_module;
    py::object py_class;
    py::object py_instance;
};

ROBOTICK_REGISTER_WORKLOAD(PythonWorkload, PythonConfig, PythonInputs, PythonOutputs);
