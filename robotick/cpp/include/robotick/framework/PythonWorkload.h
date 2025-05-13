#pragma once

#include "robotick/framework/api.h"
#include "robotick/framework/WorkloadBase.h"
#include <memory>

namespace robotick
{

    class ROBOTICK_API PythonWorkload : public WorkloadBase
    {
    public:
        PythonWorkload(std::string name, std::string module, std::string cls, double tick_rate_hz);
        ~PythonWorkload() override;

        void tick(const InputBlock &in, OutputBlock &out) override;

    private:
        ROBOTICK_DECLARE_PIMPL();
    };

}
