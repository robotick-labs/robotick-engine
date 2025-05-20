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


#pragma once

#include "robotick/framework/Model.h"
#include "robotick/framework/api.h"

#include <atomic>
#include <memory>
#include <string>

namespace robotick
{
	struct WorkloadRegistryEntry;

	struct WorkloadInstanceInfo
	{
		void* ptr;
		const WorkloadRegistryEntry* type;
		std::string unique_name;
		double tick_rate_hz;
		const std::vector<const WorkloadInstanceInfo*> children;
	};

	namespace test
	{
		struct EngineInspector;
	}

	class ROBOTICK_API Engine
	{
		friend struct robotick::test::EngineInspector;

	  public:
		Engine();
		~Engine();

		void load(const Model& model);
		void run(const std::atomic<bool>& stop_flag);

	  protected:
		const WorkloadInstanceInfo& get_instance_info(size_t index) const;

	  private:
		ROBOTICK_DECLARE_PIMPL();
	};
} // namespace robotick
