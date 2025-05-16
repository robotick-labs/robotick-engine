#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

using namespace robotick;

struct SyncedPairConfig
{
	WorkloadHandle primary;
	WorkloadHandle secondary;
	double tick_rate_hz = 30.0;
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(SyncedPairConfig, ROBOTICK_FIELD(SyncedPairConfig, primary),
					   ROBOTICK_FIELD(SyncedPairConfig, secondary), ROBOTICK_FIELD(SyncedPairConfig, tick_rate_hz))

class SyncedPairWorkload
{
  public:
	SyncedPairConfig config;

	void *primary_ptr = nullptr;
	void *secondary_ptr = nullptr;
	void (*primary_tick)(void *ptr, double time_delta) = nullptr;
	void (*secondary_tick)(void *, double time_delta) = nullptr;

	void setup(const Model &model)
	{
		const auto &inst = model.factory().get_all();
		primary_ptr = inst[config.primary.index].ptr;
		primary_tick = inst[config.primary.index].type->tick;
		secondary_ptr = inst[config.secondary.index].ptr;
		secondary_tick = inst[config.secondary.index].type->tick;
	}

	void tick(double time_delta)
	{
		primary_tick(primary_ptr, time_delta);
		secondary_tick(secondary_ptr, time_delta);
	}

	double get_tick_rate_hz() const
	{
		return config.tick_rate_hz;
	}
};

ROBOTICK_REGISTER_WORKLOAD(SyncedPairWorkload, SyncedPairConfig, robotick::EmptyInputs, robotick::EmptyOutputs);
