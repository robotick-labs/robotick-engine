#pragma once

#include "robotick/framework/WorkloadBase.h"
#include <vector>
#include <memory>

namespace robotick {

    class SequenceWorkload : public WorkloadBase {
    public:
        SequenceWorkload(std::string name)
        : WorkloadBase(std::move(name)) {}

        void add(std::shared_ptr<IWorkload> workload) {
            m_sequence.push_back(std::move(workload));
        }

        double get_tick_rate_hz() override {
            return m_sequence.empty() ? 0.0 : m_sequence.front()->get_tick_rate_hz();
        }

        void tick(const InputBlock& in, OutputBlock& out) override {
            for (auto& workload : m_sequence) {
                workload->tick(in, out);
            }
        }

    private:
        std::vector<std::shared_ptr<IWorkload>> m_sequence;
    };

}
