// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/Workload.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/FixedString.h"

namespace robotick
{

	//------------------------------------------------------------------------------
	// Template field structs with all supported field types
	//------------------------------------------------------------------------------

	struct TemplateConfig
	{
		double gain;
		int threshold;
		FixedString32 label;
		Blackboard config_blackboard; // you would tend to only use a Blackboard on a scripting Workload where you don't know your fields in advance -
									  // more indirection means they are a bit slower to use
	};
	ROBOTICK_BEGIN_FIELDS(TemplateConfig)
	ROBOTICK_FIELD(TemplateConfig, gain)
	ROBOTICK_FIELD(TemplateConfig, threshold)
	ROBOTICK_FIELD(TemplateConfig, label)
	ROBOTICK_END_FIELDS()

	struct TemplateInput
	{
		double angle;
		int sensor_value;
		FixedString16 sensor_label;
		Blackboard input_blackboard; // (see note on TemplateConfig::config_blackboard)
	};
	ROBOTICK_BEGIN_FIELDS(TemplateInput)
	ROBOTICK_FIELD(TemplateInput, double, angle)
	ROBOTICK_FIELD(TemplateInput, int, sensor_value)
	ROBOTICK_FIELD(TemplateInput, FixedString16, sensor_label)
	ROBOTICK_FIELD(TemplateInput, Blackboard, input_blackboard)
	ROBOTICK_END_FIELDS()

	struct TemplateOutput
	{
		double command;
		FixedString64 status;
		Blackboard output_blackboard; // (see note on TemplateConfig::config_blackboard)

		bool has_called_set_children = false;
		bool has_called_set_engine = false;
		bool has_called_pre_load = false;
		bool has_called_load = false;
		bool has_called_setup = false;
		bool has_called_start = false;
		bool has_called_tick = false;
		bool has_called_stop = false;
	};
	ROBOTICK_BEGIN_FIELDS(TemplateOutput)
	ROBOTICK_FIELD(TemplateOutput, double, command)
	ROBOTICK_FIELD(TemplateOutput, FixedString64, status)
	ROBOTICK_FIELD(TemplateOutput, Blackboard, status)
	ROBOTICK_END_FIELDS()

	//------------------------------------------------------------------------------
	// TemplateWorkload: demonstrates full function pointer coverage
	//------------------------------------------------------------------------------

	struct TemplateWorkload
	{
		TemplateConfig config;
		TemplateInput inputs;
		TemplateOutput outputs;

		void set_children(const std::vector<const WorkloadInstanceInfo*>& children, std::vector<DataConnectionInfo*>& connections)
		{
			// Handle child linkage (if applicable - typically only used for compositional workloads
			// e.g. AsyncPairWorkload, SyncedGroupWorkloads, SequencedGroupWorkload)

			outputs.has_called_set_children = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}

		void set_engine(const Engine& engine)
		{
			// Access engine if needed (avoid unless you know what you're doing - typically only used on telemetry- and compositional workloads)

			outputs.has_called_set_engine = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}

		void pre_load()
		{
			// Called before blackboards or memory allocated...

			// ... meaning it is the correct place to set the schema for each of our blackboards, for example:
			config.blackboard = Blackboard({BlackboardFieldInfo("my_config_double", "double")});
			inputs.blackboard = Blackboard({BlackboardFieldInfo("my_config_int", "int"), BlackboardFieldInfo("my_config_string", "FixedString64")});
			outputs.blackboard = Blackboard({BlackboardFieldInfo("my_config_string", "FixedString64")});

			outputs.has_called_preload = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}

		void load()
		{
			// Called after blackboards allocated, safe to inspect config/inputs/outputs
		}

		void setup()
		{
			// Called once before first tick
		}

		void start(double time_now)
		{
			// Called once when ticking begins
		}

		void tick(double delta_time)
		{
			// Main tick loop
		}

		void stop()
		{
			// Called after ticking has stopped
		}
	};

} // namespace robotick
