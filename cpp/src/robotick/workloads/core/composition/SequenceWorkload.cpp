#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include <string>
#include <vector>

using namespace robotick;

struct SequenceConfig
{
	std::vector<WorkloadHandle> children;
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(SequenceConfig, ROBOTICK_FIELD(SequenceConfig, children))

struct SequenceWorkload
{
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
			child_ticks.push_back(all[h.index].type->tick_fn);
		}
	}

	void tick(double time_delta)
	{
		for (size_t i = 0; i < child_ptrs.size(); ++i)
			child_ticks[i](child_ptrs[i], time_delta);
	}
};

static WorkloadAutoRegister<SequenceWorkload, SequenceConfig> s_auto_register;
