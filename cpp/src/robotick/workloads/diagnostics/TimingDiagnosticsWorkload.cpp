// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

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
	ROBOTICK_REGISTER_STRUCT_BEGIN(TimingDiagnosticsConfig)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsConfig, int, log_rate_hz)
	ROBOTICK_REGISTER_STRUCT_END(TimingDiagnosticsConfig

	struct TimingDiagnosticsInputs
	{
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(TimingDiagnosticsInputs)
	ROBOTICK_REGISTER_STRUCT_END(TimingDiagnosticsInputs)

	struct TimingDiagnosticsOutputs
	{
		double last_tick_rate = 0.0;
		double avg_tick_rate = 0.0;
		double tick_stddev = 0.0;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(TimingDiagnosticsOutputs)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsOutputs, double, last_tick_rate)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsOutputs, double, avg_tick_rate)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsOutputs, double, tick_stddev)
	ROBOTICK_REGISTER_STRUCT_END(TimingDiagnosticsOutputs)

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

		void tick(const TickInfo& tick_info)
		{
			if (config.log_rate_hz == 0 || tick_info.delta_time <= 0.0)
			{
				return;
			}

			const double actual_dt = tick_info.delta_time;
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

	ROBOTICK_REGISTER_WORKLOAD(TimingDiagnosticsWorkload, TimingDiagnosticsConfig, TimingDiagnosticsInputs, TimingDiagnosticsOutputs)
} // namespace robotick