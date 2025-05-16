#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

struct DummyConfig
{
	double tick_rate_hz = 123.0;
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(DummyConfig, ROBOTICK_FIELD(DummyConfig, tick_rate_hz))

class DummyWorkload
{
  public:
	DummyConfig config;
	double get_tick_rate_hz() const
	{
		return config.tick_rate_hz;
	}
	void tick(double)
	{
	}
};

ROBOTICK_REGISTER_WORKLOAD(DummyWorkload, DummyConfig, robotick::EmptyInputs, robotick::EmptyOutputs);

TEST_CASE("Unit|Framework|Engine|DummyWorkload stores tick rate")
{
	Model model;
	auto h = model.add_by_type("DummyWorkload", "dummy", {{"tick_rate_hz", 123.0}});
	model.finalise();

	auto *inst = model.get<DummyWorkload>(h);
	REQUIRE(inst->get_tick_rate_hz() == 123.0);
}
