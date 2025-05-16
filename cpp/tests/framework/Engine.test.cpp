#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

struct CountConfig
{
	double tick_rate_hz = 10.0;
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(CountConfig, ROBOTICK_FIELD(CountConfig, tick_rate_hz))

class CountingWorkload
{
  public:
	CountConfig config;
	int tick_count = 0;

	double get_tick_rate_hz() const
	{
		return config.tick_rate_hz;
	}
	void tick(double)
	{
		tick_count++;
	}
};

ROBOTICK_REGISTER_WORKLOAD(CountingWorkload, CountConfig, robotick::EmptyInputs, robotick::EmptyOutputs);

TEST_CASE("Unit|Framework|Engine|Engine runs tick() loop for registered workload")
{
	Model model;
	auto h = model.add_by_type("CountingWorkload", "counter", {});
	model.finalise();

	auto *w = model.get<CountingWorkload>(h);

	Engine engine;
	engine.load(model);
	engine.setup();
	engine.start();

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	engine.stop();

	REQUIRE(w->tick_count >= 1);
}
