#pragma once

#include "robotick/framework/api.h"
#include "robotick/framework/WorkloadBase.h"
#include <memory>

struct _object; // Forward-declare PyObject

namespace robotick
{
    class ROBOTICK_API PythonWorkload : public WorkloadBase
    {
    public:
        PythonWorkload(std::string name, std::string module, std::string cls, double tick_rate_hz);
        ~PythonWorkload() override;

        void tick(const InputBlock &in, OutputBlock &out, double time_delta) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
