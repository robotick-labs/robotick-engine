// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"

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
	ROBOTICK_REGISTER_STRUCT_BEGIN(HelloConfig)
	ROBOTICK_STRUCT_FIELD(HelloConfig, double, multiplier)
	ROBOTICK_REGISTER_STRUCT_END(HelloConfig)

	struct HelloInputs
	{
		double a = 0.0;
		double b = 0.0;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(HelloInputs)
	ROBOTICK_STRUCT_FIELD(HelloInputs, double, a)
	ROBOTICK_STRUCT_FIELD(HelloInputs, double, b)
	ROBOTICK_REGISTER_STRUCT_END(HelloInputs)

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

	ROBOTICK_REGISTER_STRUCT_BEGIN(HelloOutputs)
	ROBOTICK_STRUCT_FIELD(HelloOutputs, double, sum)
	ROBOTICK_STRUCT_FIELD(HelloOutputs, FixedString32, message)
	ROBOTICK_STRUCT_FIELD(HelloOutputs, int, status)
	ROBOTICK_REGISTER_STRUCT_END(HelloOutputs)

	// === Workload ===

	struct HelloWorkload
	{
		HelloInputs inputs;
		HelloOutputs outputs;
		HelloConfig config;

		void tick(const TickInfo&)
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

	ROBOTICK_REGISTER_WORKLOAD(HelloWorkload, HelloConfig, HelloInputs, HelloOutputs)

} // namespace robotick
