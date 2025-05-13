// ===============================
// SyncedPairWorkload.h
// ===============================
#pragma once

#include "robotick/framework/WorkloadBase.h"
#include <memory>

namespace robotick
{

    class ROBOTICK_API SyncedPairWorkload : public WorkloadBase
    {
    public:
        SyncedPairWorkload(std::string name,
                           std::shared_ptr<IWorkload> primary,
                           std::shared_ptr<IWorkload> secondary);
        ~SyncedPairWorkload() override;

        double get_tick_rate_hz() override;
        void load() override;
        void setup() override;
        void tick(const InputBlock &in, OutputBlock &out) override;
        void stop() override;

    private:
        ROBOTICK_DECLARE_PIMPL();
    };

}