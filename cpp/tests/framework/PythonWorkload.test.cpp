
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

TEST_CASE("Unit|Workloads|PythonWorkload|PythonWorkload can tick without crash")
{
	Model model;
	auto h = model.add_by_type("PythonWorkload", "py",
							   {{"script_name", "robotick.workloads.optional.test.hello_workload"},
								{"class_name", "HelloWorkload"},
								{"tick_rate_hz", 5.0}});
	model.finalise();

	auto *ptr = model.factory().get_raw_ptr(h);
	auto *entry = get_workload_registry_entry(model.factory().get_type_name(h));

	REQUIRE(entry); // fail early if unregistered

	REQUIRE_NOTHROW(entry->load(ptr));
	REQUIRE_NOTHROW(entry->tick(ptr, 0.01));
}
