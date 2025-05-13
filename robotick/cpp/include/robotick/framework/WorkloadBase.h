#pragma once

#include "robotick/framework/IWorkload.h"

namespace robotick
{

    class ROBOTICK_API WorkloadBase : public IWorkload
    {
    public:
        WorkloadBase(std::string name, double tick_rate_hz = 0.0)
            : m_name(std::move(name)), m_tick_rate_hz(tick_rate_hz) {}

        double get_tick_rate_hz() override
        {
            return m_tick_rate_hz;
        }

        std::string get_name() override
        {
            return m_name;
        }

    protected:
        std::string m_name;
        double m_tick_rate_hz;
    };

}
