#include <robotick/framework/PythonWorkload.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace robotick;

TEST_CASE("PythonWorkload - Constructs with valid Python class") {
    CHECK_NOTHROW(PythonWorkload("hello", "robotick.workloads.optional.test.hello_workload", "HelloWorkload", 10.0));
}

TEST_CASE("PythonWorkload - Fails gracefully with invalid module or class") {
    CHECK_NOTHROW(PythonWorkload("invalid", "nonexistent_module", "BogusClass", 10.0));

    PythonWorkload wl("fail_class", "robotick.workloads.optional.test.hello_workload", "NoSuchClass", 10.0);

    InputBlock in;
    OutputBlock out;
    CHECK_NOTHROW(wl.tick(in, out, 0.1));
}

TEST_CASE("PythonWorkload - tick() calls Python and receives output") {
    PythonWorkload wl("hello", "robotick.workloads.optional.test.hello_workload", "HelloWorkload", 10.0);

    InputBlock in;
    in.writable["dummy"] = 123.0;

    OutputBlock out;
    wl.tick(in, out, 0.05);

    REQUIRE(out.readable.count("greeting") > 0);
    CHECK(out.readable["greeting"] == Catch::Approx(42.0));
}

TEST_CASE("PythonWorkload - tick() handles missing output gracefully") {
    PythonWorkload wl("hello", "robotick.workloads.optional.test.hello_workload", "HelloWorkload", 10.0);

    InputBlock in;
    in.writable["no_output"] = 1.0;

    OutputBlock out;
    wl.tick(in, out, 0.05);

    // Should be empty
    CHECK(out.readable.empty());
}

TEST_CASE("PythonWorkload - tick() handles Python error gracefully") {
    PythonWorkload wl("hello", "robotick.workloads.optional.test.hello_workload", "HelloWorkload", 10.0);

    InputBlock in;
    in.writable["force_error"] = 1.0;

    OutputBlock out;
    CHECK_NOTHROW(wl.tick(in, out, 0.01));

    // Should still be safe
    CHECK(out.readable.empty());
}

TEST_CASE("PythonWorkload - tick() works with empty input and no crash") {
    PythonWorkload wl("hello", "robotick.workloads.optional.test.hello_workload", "HelloWorkload", 10.0);

    InputBlock in;
    OutputBlock out;

    CHECK_NOTHROW(wl.tick(in, out, 0.02));
    // No output is fine
}

TEST_CASE("PythonWorkload - tick() propagates multiple float outputs correctly") {
    PythonWorkload wl("hello", "robotick.workloads.optional.test.hello_workload", "HelloWorkload", 10.0);

    InputBlock in;
    in.writable["output_all"] = 1.0;

    OutputBlock out;
    wl.tick(in, out, 0.1);

    CHECK(out.readable["val1"] == Catch::Approx(1.23));
    CHECK(out.readable["val2"] == Catch::Approx(4.56));
}
