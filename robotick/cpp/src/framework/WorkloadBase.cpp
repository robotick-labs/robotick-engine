// ===============================
// WorkloadBase.cpp
// ===============================
#include "robotick/framework/WorkloadBase.h"

namespace robotick
{

    struct WorkloadBase::Impl
    {
        std::string name;
        double tick_rate_hz = 0.0;
    };

    WorkloadBase::WorkloadBase(std::string name, double tick_rate_hz)
        : m_impl(std::make_unique<Impl>())
    {
        m_impl->name = std::move(name);
        m_impl->tick_rate_hz = tick_rate_hz;
    }

    WorkloadBase::~WorkloadBase() = default;

    double WorkloadBase::get_tick_rate_hz()
    {
        return m_impl->tick_rate_hz;
    }

    std::string WorkloadBase::get_name()
    {
        return m_impl->name;
    }

}