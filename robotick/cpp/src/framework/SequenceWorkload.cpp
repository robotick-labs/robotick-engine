// ===============================
// SequenceWorkload.cpp
// ===============================
#include "robotick/framework/SequenceWorkload.h"
#include <vector>

namespace robotick
{

    struct SequenceWorkload::Impl
    {
        std::vector<std::shared_ptr<IWorkload>> sequence;
    };

    SequenceWorkload::SequenceWorkload(std::string name)
        : WorkloadBase(std::move(name)), m_impl(std::make_unique<Impl>()) {}

    SequenceWorkload::~SequenceWorkload() = default;

    void SequenceWorkload::add(std::shared_ptr<IWorkload> workload)
    {
        m_impl->sequence.push_back(std::move(workload));
    }

    double SequenceWorkload::get_tick_rate_hz()
    {
        return m_impl->sequence.empty() ? 0.0 : m_impl->sequence.front()->get_tick_rate_hz();
    }

    void SequenceWorkload::tick(const InputBlock &in, OutputBlock &out, double time_delta)
    {
        for (auto &w : m_impl->sequence)
        {
            w->tick(in, out, time_delta);
        }
    }

}