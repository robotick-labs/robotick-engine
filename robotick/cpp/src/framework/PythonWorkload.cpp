#include "robotick/framework/PythonWorkload.h"
#include <iostream>

namespace robotick {

    struct PythonWorkload::Impl {
        std::string module;
        std::string cls;
    };

    PythonWorkload::PythonWorkload(std::string name, std::string module, std::string cls, double tick_rate_hz)
        : WorkloadBase(std::move(name), tick_rate_hz),
          m_impl(std::make_unique<Impl>()) {
        m_impl->module = std::move(module);
        m_impl->cls = std::move(cls);
    }

    PythonWorkload::~PythonWorkload() = default;

    void PythonWorkload::tick(const InputBlock&, OutputBlock&) {
        std::cout << "[Python] " << get_name() << " tick (" << m_impl->module << "." << m_impl->cls << ")\n";
    }

}
