// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include <chrono>
#include <cmath>
#include <iostream>

namespace robotick
{

	// === Field registrations ===

	struct TimingDiagnosticsConfig
	{
		int log_rate_hz = 1;
	};
	ROBOTICK_BEGIN_FIELDS(TimingDiagnosticsConfig)
	ROBOTICK_FIELD(TimingDiagnosticsConfig, log_rate_hz)
	ROBOTICK_END_FIELDS()

	struct TimingDiagnosticsInputs
	{
	};
	ROBOTICK_BEGIN_FIELDS(TimingDiagnosticsInputs)
	ROBOTICK_END_FIELDS()

	struct TimingDiagnosticsOutputs
	{
		double last_tick_rate = 0.0;
		double avg_tick_rate = 0.0;
		double tick_stddev = 0.0;
	};
	ROBOTICK_BEGIN_FIELDS(TimingDiagnosticsOutputs)
	ROBOTICK_FIELD(TimingDiagnosticsOutputs, last_tick_rate)
	ROBOTICK_FIELD(TimingDiagnosticsOutputs, avg_tick_rate)
	ROBOTICK_FIELD(TimingDiagnosticsOutputs, tick_stddev)
	ROBOTICK_END_FIELDS()

	// === Internal state (not registered) ===

	struct TimingDiagnosticsInternalState
	{
		int count = 0;
		double sum_dt = 0.0;
		double sum_dt2 = 0.0;
	};

	// === Workload ===

	struct TimingDiagnosticsWorkload
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
		}

		void tick(double time_delta)
		{
			if (config.log_rate_hz == 0 || time_delta <= 0.0)
			{
				return;
			}

			const double actual_dt = time_delta;
			const double tick_rate = 1.0 / actual_dt;

			outputs.last_tick_rate = tick_rate;

			internal_state.count++;
			internal_state.sum_dt += actual_dt;
			internal_state.sum_dt2 += actual_dt * actual_dt;

			const double tick_period = 1.0 / config.log_rate_hz;

			if (internal_state.sum_dt >= tick_period)
			{
				double mean_dt = internal_state.sum_dt / internal_state.count;
				double mean_dt2 = internal_state.sum_dt2 / internal_state.count;
				const double variance = std::max(0.0, mean_dt2 - mean_dt * mean_dt);
				const double stddev = std::sqrt(variance);

				outputs.avg_tick_rate = 1.0 / mean_dt;
				outputs.tick_stddev = stddev;

				static constexpr double seconds_to_microseconds = 1e6;

				std::cerr << std::fixed;
				std::cerr << "[TimingDiagnostics] avg: " << outputs.avg_tick_rate << " Hz, stddev: " << outputs.tick_stddev * seconds_to_microseconds
						  << " µs\n";

				internal_state.count = 0;
				internal_state.sum_dt = 0.0;
				internal_state.sum_dt2 = 0.0;
			}
		}
	};

	// === Auto-registration ===

	ROBOTICK_DEFINE_WORKLOAD(TimingDiagnosticsWorkload)
} // namespace robotick