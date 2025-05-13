// ===============================
// SequenceWorkload.h
// ===============================
#pragma once

#include "robotick/framework/WorkloadBase.h"
#include <memory>

namespace robotick
{

    class ROBOTICK_API SequenceWorkload : public WorkloadBase
    {
    public:
        SequenceWorkload(std::string name);
        ~SequenceWorkload() override;

        void add(std::shared_ptr<IWorkload> workload);
        double get_tick_rate_hz() override;
        void tick(const InputBlock &in, OutputBlock &out) override;

    private:
        ROBOTICK_DECLARE_PIMPL();
    };

}