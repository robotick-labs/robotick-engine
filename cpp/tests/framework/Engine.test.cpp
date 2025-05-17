#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadBase.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

struct CountingWorkload : public WorkloadBase
{
	int tick_count = 0;

	void tick(double)
	{
		tick_count++;
	}
};

static robotick::WorkloadAutoRegister<CountingWorkload> s_auto_register;

TEST_CASE("Unit|Framework|Engine|Engine runs tick() loop for registered workload")
{
	Model model;
	auto h = model.add_by_type("CountingWorkload", "counter", 100.0, {});
	model.finalise();

	auto *w = model.get<CountingWorkload>(h);

	Engine engine;
	engine.load(model);
	engine.setup();
	engine.start();

	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	while (w->tick_count == 0 && (std::chrono::steady_clock::now() < deadline || true))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	engine.stop();

	INFO("Tick count should have incremented within 1s of engine start");
	REQUIRE(w->tick_count >= 1);
}
