
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "utils/EngineInspector.h"

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <sstream>
#include <streambuf>

using namespace robotick;
using namespace robotick::test_access;

TEST_CASE("Unit|Workloads|PythonWorkload|PythonWorkload can tick without crash")
{
	Model model;
	model.add("PythonWorkload", "py", 5.0,
			  {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});

	Engine engine;
	REQUIRE_NOTHROW(engine.load(model));

	const WorkloadInstanceInfo* instance_info = nullptr;
	REQUIRE_NOTHROW(instance_info = &EngineInspector::get_instance_info(engine, 0)); // fail early if unregistered

	// Try ticking it manually
	REQUIRE(instance_info->ptr); // fail early if unregistered
	REQUIRE_NOTHROW(instance_info->type->tick_fn(instance_info->ptr, 0.01));

	REQUIRE_NOTHROW(engine.stop()); // we must clean up properly else we'll get exceptions
}
