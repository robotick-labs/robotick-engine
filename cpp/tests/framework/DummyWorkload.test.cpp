#include "robotick/framework/Model.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

struct DummyWorkload
{
	void tick(double)
	{
	}
};

static robotick::WorkloadAutoRegister<DummyWorkload> s_auto_register;

TEST_CASE("Unit|Framework|Engine|DummyWorkload stores tick rate")
{
	Model model;
	auto h = model.add_by_type("DummyWorkload", "dummy", 123.0, {});
	model.finalise();

	auto &inst = model.get_instance(h);
	REQUIRE(inst.tick_rate_hz == 123.0);
}
