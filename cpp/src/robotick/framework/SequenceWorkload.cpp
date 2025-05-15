#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadMacros.h"

#include <vector>
#include <string>

using namespace robotick;

struct SequenceConfig
{
    std::vector<WorkloadHandle> children;
    double tick_rate_hz = 30.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(SequenceConfig,
                       ROBOTICK_FIELD(SequenceConfig, children),
                       ROBOTICK_FIELD(SequenceConfig, tick_rate_hz))

class SequenceWorkload
{
public:
    SequenceConfig config;
    std::vector<void *> child_ptrs;
    std::vector<void (*)(void *, double time_delta)> child_ticks;

    void setup(const Model &model)
    {
        const auto &factory = model.factory();
        const auto &all = factory.get_all();

        for (auto h : config.children)
        {
            child_ptrs.push_back(all[h.index].ptr);
            child_ticks.push_back(all[h.index].type->tick);
        }
    }

    void tick(double time_delta)
    {
        for (size_t i = 0; i < child_ptrs.size(); ++i)
            child_ticks[i](child_ptrs[i], time_delta);
    }

    double get_tick_rate_hz() const
    {
        return config.tick_rate_hz;
    }
};

ROBOTICK_REGISTER_WORKLOAD(SequenceWorkload, SequenceConfig, robotick::EmptyInputs, robotick::EmptyOutputs);
