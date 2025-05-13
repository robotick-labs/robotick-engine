#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "robotick/framework/WorkloadBase.h"

using namespace robotick;

class DummyWorkload : public WorkloadBase {
public:
    DummyWorkload(std::string name, double rate) : WorkloadBase(std::move(name), rate) {}
    void tick(const InputBlock&, OutputBlock&) override {}
};

TEST_CASE("WorkloadBase stores name and tick rate") {
    DummyWorkload w("test_workload", 42.5);

    REQUIRE(w.get_name() == "test_workload");
    REQUIRE(w.get_tick_rate_hz() == Catch::Approx(42.5));
}
