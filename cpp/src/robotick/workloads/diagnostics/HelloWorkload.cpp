// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include <cstdio>
#include <iomanip>
#include <sstream>

namespace robotick
{

	// === Field registrations ===

	struct HelloConfig
	{
		double multiplier = 1.0;
	};
	ROBOTICK_BEGIN_FIELDS(HelloConfig)
	ROBOTICK_FIELD(HelloConfig, multiplier)
	ROBOTICK_END_FIELDS()

	struct HelloInputs
	{
		double a = 0.0;
		double b = 0.0;
	};
	ROBOTICK_BEGIN_FIELDS(HelloInputs)
	ROBOTICK_FIELD(HelloInputs, a)
	ROBOTICK_FIELD(HelloInputs, b)
	ROBOTICK_END_FIELDS()

	enum class HelloStatus
	{
		NORMAL,
		MAGIC
	};

	struct HelloOutputs
	{
		double sum = 0.0;
		FixedString32 message = "Waiting...";
		HelloStatus status = HelloStatus::NORMAL;
	};

	ROBOTICK_BEGIN_FIELDS(HelloOutputs)
	ROBOTICK_FIELD(HelloOutputs, sum)
	ROBOTICK_FIELD(HelloOutputs, message)
	ROBOTICK_FIELD(HelloOutputs, status)
	ROBOTICK_END_FIELDS()

	// === Workload ===

	struct HelloWorkload
	{
		HelloInputs inputs;
		HelloOutputs outputs;
		HelloConfig config;

		void tick(double)
		{
			outputs.sum = (inputs.a + inputs.b) * config.multiplier;

			if (outputs.sum == 42.0)
			{
				outputs.message = "The Answer!";
				outputs.status = HelloStatus::MAGIC;
			}
			else
			{
				std::ostringstream oss;
				oss << "Sum = " << std::fixed << std::setprecision(2) << outputs.sum;
				outputs.message = oss.str().c_str();
				outputs.status = HelloStatus::NORMAL;
			}
		}
	};

	// === Auto-registration ===

	ROBOTICK_DEFINE_WORKLOAD(HelloWorkload)

} // namespace robotick
