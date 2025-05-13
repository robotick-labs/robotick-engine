#include "robotick/framework/PythonWorkload.h"
#include <iostream>

namespace robotick
{

    PythonWorkload::PythonWorkload(std::string name, std::string module, std::string cls, double tick_rate_hz)
    : WorkloadBase(std::move(name), tick_rate_hz),
      m_module(std::move(module)),
      m_class(std::move(cls)) {}

    PythonWorkload::~PythonWorkload() {}

    void PythonWorkload::tick(const InputBlock &, OutputBlock &)
    {
        std::cout << "[Python] " << m_name << " tick (" << m_module << "." << m_class << ")\n";
    }
}
