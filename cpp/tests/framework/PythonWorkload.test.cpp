// Copyright 2025 Robotick Labs
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

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "utils/EngineInspector.h"

#include <catch2/catch_test_macros.hpp>

using namespace robotick;
using namespace robotick::test;

TEST_CASE("Unit|Workloads|PythonWorkload|PythonWorkload can start, tick and stop without crash")
{
	Model model;
	const auto handle =
		model.add("PythonWorkload", "py", 5.0, {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});
	model.set_root(handle);

	Engine engine;
	REQUIRE_NOTHROW(engine.load(model));

	const auto& info = EngineInspector::get_instance_info(engine, handle.index);

	const double tick_rate_hz = 100.0;
	const double time_delta = 1.0 / tick_rate_hz;

	REQUIRE(info.ptr != nullptr);
	REQUIRE(info.type != nullptr);

	if (info.type->start_fn != nullptr)
	{
		REQUIRE_NOTHROW(info.type->start_fn(info.ptr, tick_rate_hz));
	}

	REQUIRE_NOTHROW(info.type->tick_fn(info.ptr, time_delta));

	if (info.type->stop_fn != nullptr)
	{
		REQUIRE_NOTHROW(info.type->stop_fn(info.ptr));
	}
}
