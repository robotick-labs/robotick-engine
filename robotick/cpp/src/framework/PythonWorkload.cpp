#include "robotick/framework/PythonWorkload.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <iostream>

namespace py = pybind11;

namespace robotick {

    struct PythonWorkload::Impl {
        std::string module;
        std::string cls;

        py::scoped_interpreter guard{};
        py::object py_instance;

        Impl(const std::string& mod, const std::string& clsname) {
            try {
                auto py_module = py::module_::import(mod.c_str());
                auto py_class = py_module.attr(clsname.c_str());
                py_instance = py_class();  // Call constructor
                std::cout << "[Python] Loaded " << mod << "." << clsname << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[Python ERROR] Failed to load class: " << e.what() << "\n";
            }
        }
    };

    PythonWorkload::PythonWorkload(std::string name, std::string module, std::string cls, double tick_rate_hz)
        : WorkloadBase(std::move(name), tick_rate_hz),
          m_impl(std::make_unique<Impl>(module, cls)) {}

    PythonWorkload::~PythonWorkload() = default;

    void PythonWorkload::tick(const InputBlock& in, OutputBlock& out, double time_delta) {
        if (!m_impl->py_instance) {
            std::cerr << "[Python ERROR] Python instance not available\n";
            return;
        }

        // Map InputBlock to dict
        py::dict py_input;
        for (const auto& [key, val] : in.writable)
            py_input[py::str(key)] = val;

        // Create empty output dict
        py::dict py_output;

        try {
            m_impl->py_instance.attr("tick")(time_delta, py_input, py_output);
        } catch (const py::error_already_set& e) {
            std::cerr << "[Python ERROR] Exception in tick: " << e.what() << "\n";
            return;
        }

        // Map py_output back to OutputBlock
        for (const auto& item : py_output) {
            std::string key = py::str(item.first);
            double val = py::cast<double>(item.second); 
            out.readable[key] = val;
        }
    }

}
