#include "robotick/framework/registry/WorkloadFactory.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <any>
#include <catch2/catch_all.hpp>  // Optional — pulls in everything (larger header)
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <map>
#include <string>

using namespace Catch;
using namespace robotick;

namespace
{
    struct DummyConfig
    {
        int value = 0;
        ROBOTICK_DECLARE_FIELDS();
    };

    ROBOTICK_DEFINE_FIELDS(DummyConfig, ROBOTICK_FIELD(DummyConfig, value))

    // Dummy workload for testing
    struct Dummy
    {
        DummyConfig config;
    };

    void dummy_construct(void* ptr)
    {
        new (ptr) Dummy();
    }

    void register_dummy_type()
    {
        static const WorkloadRegistryEntry entry = {"DummyWorkload",
                                                    sizeof(Dummy),
                                                    alignof(Dummy),
                                                    dummy_construct,
                                                    DummyConfig::get_struct_reflection(),
                                                    offsetof(Dummy, config),
                                                    nullptr,
                                                    0,
                                                    nullptr,
                                                    0,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr};

        static WorkloadRegistration<Dummy> reg{entry};
    }

}  // namespace

TEST_CASE("Unit|Framework|WorkloadFactory|DummyWorkload stores tick rate")
{
    register_dummy_type();
    WorkloadFactory factory;
    factory.add("DummyWorkload", "ticky", 25.0, {});
    factory.finalise();

    REQUIRE(factory.get_all()[0].tick_rate_hz == Approx(25.0));
}

TEST_CASE("Unit|Framework|WorkloadFactory|Duplicate name is skipped")
{
    register_dummy_type();
    WorkloadFactory factory;
    factory.add("DummyWorkload", "duplicate", 30.0, {});
    factory.add("DummyWorkload", "duplicate", 60.0, {});  // same name
    factory.finalise();

    REQUIRE(factory.get_all().size() == 1);
    REQUIRE(factory.get_all()[0].unique_name == "duplicate");
}

TEST_CASE("Unit|Framework|WorkloadFactory|Throws on finalise twice")
{
    register_dummy_type();
    WorkloadFactory factory;
    factory.add("DummyWorkload", "x", 1.0, {});
    factory.finalise();

    REQUIRE_THROWS_WITH(factory.finalise(), "Already finalised");
}

TEST_CASE("Unit|Framework|WorkloadFactory|Throws on add after finalise")
{
    register_dummy_type();
    WorkloadFactory factory;
    factory.add("DummyWorkload", "x", 1.0, {});
    factory.finalise();

    REQUIRE_THROWS_WITH(factory.add("DummyWorkload", "y", 2.0, {}), "Factory already finalised");
}

TEST_CASE("Unit|Framework|WorkloadFactory|Throws on unknown type")
{
    WorkloadFactory factory;
    REQUIRE_THROWS_WITH(factory.add("UnknownType", "x", 1.0, {}), "Unknown workload type: UnknownType");
}

TEST_CASE("Unit|Framework|WorkloadFactory|Applies config values to struct")
{
    register_dummy_type();
    WorkloadFactory factory;
    factory.add("DummyWorkload", "with_config", 1.0, {{"value", 123}});
    factory.finalise();

    auto* raw_ptr = static_cast<const Dummy*>(factory.get_raw_ptr({0}));
    REQUIRE(raw_ptr->config.value == 123);
}

TEST_CASE("Unit|Framework|WorkloadFactory|get_type_name returns correct name")
{
    register_dummy_type();
    WorkloadFactory factory;
    auto            handle = factory.add("DummyWorkload", "t1", 5.0, {});
    factory.finalise();

    REQUIRE(factory.get_type_name(handle) == std::string("DummyWorkload"));
}
