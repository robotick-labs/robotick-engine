
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <sstream>
#include <streambuf>

using namespace robotick;

TEST_CASE("Unit|Workloads|PythonWorkload|PythonWorkload can tick without crash")
{
    Model model;
    auto  h = model.add("PythonWorkload", "py", 5.0,
                        {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});
    model.finalise();

    auto* ptr = model.factory().get_raw_ptr(h);
    auto* entry = get_workload_registry_entry(model.factory().get_type_name(h));

    REQUIRE(entry);  // fail early if unregistered

    REQUIRE_NOTHROW(entry->load_fn(ptr));
    REQUIRE_NOTHROW(entry->tick_fn(ptr, 0.01));
}
