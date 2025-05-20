// Copyright 2025 Robotick Labs CIC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "robotick/framework/FixedString.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include <cstdint>
#include <cstdio>
#include <string>

using namespace robotick;

// === Reflectable Structs ===

struct HelloConfig
{
	double multiplier = 1.0;
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(HelloConfig, ROBOTICK_FIELD(HelloConfig, multiplier))

struct HelloInputs
{
	double a = 0.0;
	double b = 0.0;
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(HelloInputs, ROBOTICK_FIELD(HelloInputs, a), ROBOTICK_FIELD(HelloInputs, b))

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
	ROBOTICK_DECLARE_FIELDS();
};
ROBOTICK_DEFINE_FIELDS(HelloOutputs, ROBOTICK_FIELD(HelloOutputs, sum), ROBOTICK_FIELD(HelloOutputs, message))

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
			char buf[32];
			std::snprintf(buf, sizeof(buf), "Sum = %.2f", outputs.sum);
			outputs.message = buf;
			outputs.status = HelloStatus::NORMAL;
		}
	}
};

// === Workload Auto-Registration ===
static WorkloadAutoRegister<HelloWorkload, HelloConfig, HelloInputs, HelloOutputs> s_auto_register;
