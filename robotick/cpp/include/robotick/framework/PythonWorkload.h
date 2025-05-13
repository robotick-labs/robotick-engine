#pragma once

#include "robotick/framework/api.h"
#include "robotick/framework/WorkloadBase.h"
#include <string>

namespace robotick
{

    class ROBOTICK_API PythonWorkload : public WorkloadBase
    {
    public:
        PythonWorkload(std::string name, std::string module, std::string cls, double tick_rate_hz);
        virtual ~PythonWorkload();

        void tick(const InputBlock &in, OutputBlock &out) override;

    private:
        std::string m_module;
        std::string m_class;
    };

}
