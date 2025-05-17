
#include "robotick/framework/FixedString.h"
#include "robotick/framework/WorkloadBase.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/PyBind.h"

#include <chrono>
#include <cmath>
#include <iostream>

using namespace robotick;

struct TimingDiagnosticsConfig
{
	int report_every = 100; // Number of ticks per report
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(TimingDiagnosticsConfig, ROBOTICK_FIELD(TimingDiagnosticsConfig, report_every))

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

struct TimingDiagnosticsInternalState
{
	std::chrono::steady_clock::time_point last_time;
	int count = 0;
	double sum_dt = 0.0;
	double sum_dt2 = 0.0;
};

struct TimingDiagnosticsWorkload : public WorkloadBase
{
	TimingDiagnosticsConfig config;
	TimingDiagnosticsInputs inputs;
	TimingDiagnosticsOutputs outputs;

	TimingDiagnosticsInternalState internal_state;

	void load()
	{
		internal_state.count = 0;
		internal_state.sum_dt = 0.0;
		internal_state.sum_dt2 = 0.0;
		internal_state.last_time = std::chrono::steady_clock::now();
	}

	void tick(double dt)
	{
		using namespace std;

		auto now = std::chrono::steady_clock::now();
		double actual_dt = dt;
		double tick_rate = 1.0 / actual_dt;

		outputs.last_tick_rate = tick_rate;

		internal_state.count++;
		internal_state.sum_dt += actual_dt;
		internal_state.sum_dt2 += actual_dt * actual_dt;

		if (internal_state.count >= config.report_every)
		{
			double mean_dt = internal_state.sum_dt / internal_state.count;
			double mean_dt2 = internal_state.sum_dt2 / internal_state.count;
			double stddev = sqrt(mean_dt2 - mean_dt * mean_dt);

			outputs.avg_tick_rate = 1.0 / mean_dt;
			outputs.tick_stddev = stddev;

			cerr << fixed;
			cerr << "[TimingDiagnostics] avg: " << outputs.avg_tick_rate
				 << " Hz, stddev: " << outputs.tick_stddev * 1000.0 * 1000.0 << " µs\n";

			internal_state.count = 0;
			internal_state.sum_dt = 0.0;
			internal_state.sum_dt2 = 0.0;
		}

		internal_state.last_time = now;
	}
};

static WorkloadAutoRegister<TimingDiagnosticsWorkload, TimingDiagnosticsConfig, TimingDiagnosticsInputs,
							TimingDiagnosticsOutputs>
	s_auto_register;
