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
	ROBOTICK_REGISTER_STRUCT_END(TimingDiagnosticsConfig)

	struct TimingDiagnosticsInputs
	{
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(TimingDiagnosticsInputs)
	ROBOTICK_REGISTER_STRUCT_END(TimingDiagnosticsInputs)

	struct TimingDiagnosticsOutputs
	{
		float last_tick_rate = 0.0;
		float avg_tick_rate = 0.0;
		float tick_stddev = 0.0;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(TimingDiagnosticsOutputs)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsOutputs, float, last_tick_rate)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsOutputs, float, avg_tick_rate)
	ROBOTICK_STRUCT_FIELD(TimingDiagnosticsOutputs, float, tick_stddev)
	ROBOTICK_REGISTER_STRUCT_END(TimingDiagnosticsOutputs)

	// === Internal state (not registered) ===

	struct TimingDiagnosticsInternalState
	{
		int count = 0;
		float sum_dt = 0.0;
		float sum_dt2 = 0.0;
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
			if (config.log_rate_hz == 0 || tick_info.delta_time <= 0.0f)
			{
				return;
			}

			const float actual_dt = tick_info.delta_time;
			const float tick_rate = 1.0f / actual_dt;

			outputs.last_tick_rate = tick_rate;

			internal_state.count++;
			internal_state.sum_dt += actual_dt;
			internal_state.sum_dt2 += actual_dt * actual_dt;

			const float tick_period = 1.0f / config.log_rate_hz;

			if (internal_state.sum_dt >= tick_period)
			{
				float mean_dt = internal_state.sum_dt / internal_state.count;
				float mean_dt2 = internal_state.sum_dt2 / internal_state.count;
				const float variance = std::max(0.0f, mean_dt2 - mean_dt * mean_dt);
				const float stddev = std::sqrt(variance);

				outputs.avg_tick_rate = 1.0f / mean_dt;
				outputs.tick_stddev = stddev;

				static constexpr float seconds_to_microseconds = 1e6f;

				std::cerr << std::fixed;
				std::cerr << "[TimingDiagnostics] avg: " << outputs.avg_tick_rate << " Hz, stddev: " << outputs.tick_stddev * seconds_to_microseconds
						  << " µs\n";

				internal_state.count = 0;
				internal_state.sum_dt = 0.0f;
				internal_state.sum_dt2 = 0.0f;
			}
		}
	};

	// === Auto-registration ===

	ROBOTICK_REGISTER_WORKLOAD(TimingDiagnosticsWorkload, TimingDiagnosticsConfig, TimingDiagnosticsInputs, TimingDiagnosticsOutputs)
} // namespace robotick