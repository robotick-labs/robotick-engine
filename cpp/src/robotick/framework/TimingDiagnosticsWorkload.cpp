
#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/FieldUtils.h"
#include "robotick/framework/FixedString.h"
#include "robotick/framework/WorkloadMacros.h"
#include "robotick/framework/utils_pybind.h"

#include <chrono>
#include <cmath>
#include <iostream>

using namespace robotick;

struct TimingDiagnosticsConfig
{
    double tick_rate_hz = 100.0; // Hz
    int report_every = 100;      // Number of ticks per report
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(TimingDiagnosticsConfig, ROBOTICK_FIELD(TimingDiagnosticsConfig, tick_rate_hz),
                       ROBOTICK_FIELD(TimingDiagnosticsConfig, report_every))

struct TimingDiagnosticsInputs
{
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(TimingDiagnosticsInputs)

struct TimingDiagnosticsOutputs
{
    double last_tick_rate = 0.0;
    double avg_tick_rate = 0.0;
    double tick_stddev = 0.0;
    ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(TimingDiagnosticsOutputs, ROBOTICK_FIELD(TimingDiagnosticsOutputs, last_tick_rate),
                       ROBOTICK_FIELD(TimingDiagnosticsOutputs, avg_tick_rate),
                       ROBOTICK_FIELD(TimingDiagnosticsOutputs, tick_stddev))

class TimingDiagnosticsWorkload
{
  public:
    TimingDiagnosticsConfig config;
    TimingDiagnosticsInputs inputs;
    TimingDiagnosticsOutputs outputs;

    void load()
    {
        count = 0;
        sum_dt = 0.0;
        sum_dt2 = 0.0;
        last_time = std::chrono::steady_clock::now();
    }

    void tick(double dt)
    {
        using namespace std;

        auto now = std::chrono::steady_clock::now();
        double actual_dt = dt;
        double tick_rate = 1.0 / actual_dt;

        outputs.last_tick_rate = tick_rate;

        count++;
        sum_dt += actual_dt;
        sum_dt2 += actual_dt * actual_dt;

        if (count >= config.report_every)
        {
            double mean_dt = sum_dt / count;
            double mean_dt2 = sum_dt2 / count;
            double stddev = sqrt(mean_dt2 - mean_dt * mean_dt);

            outputs.avg_tick_rate = 1.0 / mean_dt;
            outputs.tick_stddev = stddev;

            cerr << fixed;
            cerr << "[TimingDiagnostics] avg: " << outputs.avg_tick_rate
                 << " Hz, stddev: " << outputs.tick_stddev * 1000.0 << " ms\n";

            count = 0;
            sum_dt = 0.0;
            sum_dt2 = 0.0;
        }

        last_time = now;
    }

    double get_tick_rate_hz() const { return config.tick_rate_hz; }

  private:
    std::chrono::steady_clock::time_point last_time;
    int count = 0;
    double sum_dt = 0.0;
    double sum_dt2 = 0.0;
};

ROBOTICK_REGISTER_WORKLOAD(TimingDiagnosticsWorkload, TimingDiagnosticsConfig, TimingDiagnosticsInputs,
                           TimingDiagnosticsOutputs);
