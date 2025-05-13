#include <catch2/catch_test_macros.hpp>
#include "robotick/framework/SequenceWorkload.h"
#include "robotick/framework/WorkloadBase.h"

using namespace robotick;

class OrderTrackingWorkload : public WorkloadBase {
public:
    int& ref;
    int expected;
    OrderTrackingWorkload(std::string name, int& r, int expected)
        : WorkloadBase(std::move(name), 0), ref(r), expected(expected) {}

    void tick(const InputBlock&, OutputBlock&) override {
        REQUIRE(ref == expected - 1);
        ref = expected;
    }
};

TEST_CASE("SequenceWorkload ticks children in order") {
    int progress = 0;

    auto w1 = std::make_shared<OrderTrackingWorkload>("first", progress, 1);
    auto w2 = std::make_shared<OrderTrackingWorkload>("second", progress, 2);
    auto w3 = std::make_shared<OrderTrackingWorkload>("third", progress, 3);

    SequenceWorkload seq("seq");
    seq.add(w1);
    seq.add(w2);
    seq.add(w3);

    InputBlock in;
    OutputBlock out;
    seq.tick(in, out);

    REQUIRE(progress == 3);
}
