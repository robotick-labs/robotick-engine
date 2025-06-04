// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api_base.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

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
		Blackboard blackboard;
		// ^- you would tend to only use a Blackboard on a scripting Workload where you don't know your fields in advance - more indirection means
		// they are a bit slower to use
	};
	ROBOTICK_BEGIN_FIELDS(TemplateConfig)
	ROBOTICK_FIELD(TemplateConfig, double, gain)
	ROBOTICK_FIELD(TemplateConfig, int, threshold)
	ROBOTICK_FIELD(TemplateConfig, FixedString32, label)
	ROBOTICK_FIELD(TemplateConfig, Blackboard, blackboard)
	ROBOTICK_END_FIELDS()

	struct TemplateInputs
	{
		double angle;
		int sensor_value;
		FixedString16 sensor_label;
		Blackboard blackboard; // (see note on TemplateConfig::blackboard)
	};
	ROBOTICK_BEGIN_FIELDS(TemplateInputs)
	ROBOTICK_FIELD(TemplateInputs, double, angle)
	ROBOTICK_FIELD(TemplateInputs, int, sensor_value)
	ROBOTICK_FIELD(TemplateInputs, FixedString16, sensor_label)
	ROBOTICK_FIELD(TemplateInputs, Blackboard, blackboard)
	ROBOTICK_END_FIELDS()

	struct TemplateOutputs
	{
		double command;
		FixedString64 status;
		Blackboard blackboard; // (see note on TemplateConfig::blackboard)

		bool has_called_set_children = false;
		bool has_called_set_engine = false;
		bool has_called_pre_load = false;
		bool has_called_load = false;
		bool has_called_setup = false;
		bool has_called_start = false;
		bool has_called_tick = false;
		bool has_called_stop = false;
	};
	ROBOTICK_BEGIN_FIELDS(TemplateOutputs)
	ROBOTICK_FIELD(TemplateOutputs, double, command)
	ROBOTICK_FIELD(TemplateOutputs, FixedString64, status)
	ROBOTICK_FIELD(TemplateOutputs, Blackboard, blackboard)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_set_children)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_set_engine)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_pre_load)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_load)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_setup)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_start)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_tick)
	ROBOTICK_FIELD(TemplateOutputs, bool, has_called_stop)
	ROBOTICK_END_FIELDS()

	//------------------------------------------------------------------------------
	// TemplateWorkload: demonstrates full function pointer coverage
	//------------------------------------------------------------------------------

	struct TemplateWorkload
	{
		TemplateConfig config;
		TemplateInputs inputs;
		TemplateOutputs outputs;

		void set_children(const std::vector<const WorkloadInstanceInfo*>& children, std::vector<DataConnectionInfo*>& connections)
		{
			// Handle child linkage (if applicable - typically only used for compositional workloads
			// e.g. AsyncPairWorkload, SyncedGroupWorkloads, SequencedGroupWorkload)

			outputs.has_called_set_children = true; // (for unit-testing of this template - not for illustrating suggested usage!)
			(void)children;
			(void)connections; // (just to stop compiler warning is about unused args)
		}

		void set_engine(const Engine& engine)
		{
			// Access engine if needed (avoid unless you know what you're doing - typically only used on telemetry- and compositional workloads)

			outputs.has_called_set_engine = true; // (for unit-testing of this template - not for illustrating suggested usage!)
			(void)engine;						  // (just to stop compiler warning is about unused args)
		}

		void pre_load()
		{
			// Called before blackboards or memory allocated...

			// ... meaning it is the correct place to set the schema for each of our blackboards, for example:
			config.blackboard = Blackboard({BlackboardFieldInfo("my_config_double", GET_TYPE_ID(double))});

			inputs.blackboard = Blackboard(
				{BlackboardFieldInfo("my_config_int", GET_TYPE_ID(int)), BlackboardFieldInfo("my_config_string", GET_TYPE_ID(FixedString64))});

			outputs.blackboard = Blackboard({BlackboardFieldInfo("my_config_string", GET_TYPE_ID(FixedString64))});

			outputs.has_called_pre_load = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}

		void load()
		{
			// Called after blackboards allocated, safe to inspect config/inputs/outputs

			outputs.has_called_load = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}

		void setup()
		{
			// Called once before first tick

			outputs.has_called_setup = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}

		void start(double time_now)
		{
			// Called once when ticking begins

			outputs.has_called_start = true; // (for unit-testing of this template - not for illustrating suggested usage!)
			(void)time_now;					 // (just to stop compiler warning is about unused args)
		}

		void tick(double delta_time)
		{
			// Main tick loop

			outputs.has_called_tick = true; // (for unit-testing of this template - not for illustrating suggested usage!)
			(void)delta_time;				// (just to stop compiler warning is about unused args)
		}

		void stop()
		{
			// Called after ticking has stopped

			outputs.has_called_stop = true; // (for unit-testing of this template - not for illustrating suggested usage!)
		}
	};

	// === Auto-registration ===

	ROBOTICK_DEFINE_WORKLOAD(TemplateWorkload, TemplateConfig, TemplateInputs, TemplateOutputs)

} // namespace robotick
