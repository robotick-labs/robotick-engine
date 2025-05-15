#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/FieldUtils.h"
#include "robotick/framework/FixedString.h"
#include "robotick/framework/WorkloadMacros.h"

#include <string>
#include <cstdio>
#include <cstdint>

using namespace robotick;

// === Reflectable Structs ===

struct HelloConfig
{
    double multiplier = 1.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(HelloConfig,
                       ROBOTICK_FIELD(HelloConfig, multiplier))

struct HelloInputs
{
    double a = 0.0;
    double b = 0.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(HelloInputs,
                       ROBOTICK_FIELD(HelloInputs, a),
                       ROBOTICK_FIELD(HelloInputs, b))

struct HelloOutputs
{
    double sum = 0.0;
    FixedString32 message = "Waiting...";
    enum Status
    {
        NORMAL,
        MAGIC
    } status = NORMAL;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(HelloOutputs,
                       ROBOTICK_FIELD(HelloOutputs, sum),
                       ROBOTICK_FIELD(HelloOutputs, message))

// === Workload ===
class HelloWorkload
{
public:
    HelloInputs inputs;
    HelloOutputs outputs;
    HelloConfig config;

    void tick(double time_delta)
    {
        outputs.sum = (inputs.a + inputs.b) * config.multiplier;

        if (outputs.sum == 42.0)
        {
            outputs.message = "The Answer!";
            outputs.status = HelloOutputs::MAGIC;
        }
        else
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Sum = %.2f", outputs.sum);
            outputs.message = buf;
            outputs.status = HelloOutputs::NORMAL;
        }
    }

    double get_tick_rate_hz() const { return 30.0; } // dummy value
};

// === Workload Auto-Registration ===
ROBOTICK_REGISTER_WORKLOAD(HelloWorkload, HelloConfig, HelloInputs, HelloOutputs);
