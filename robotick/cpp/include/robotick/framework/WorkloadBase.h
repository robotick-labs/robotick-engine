// ===============================
// WorkloadBase.h
// ===============================
#pragma once

#include "robotick/framework/api.h"
#include "robotick/framework/IWorkload.h"
#include <memory>

namespace robotick
{

    class ROBOTICK_API WorkloadBase : public IWorkload
    {
    public:
        WorkloadBase(std::string name, double tick_rate_hz = 0.0);
        ~WorkloadBase() override;

        double get_tick_rate_hz() override;
        std::string get_name() override;

    protected:
        ROBOTICK_DECLARE_PIMPL();
    };

}
