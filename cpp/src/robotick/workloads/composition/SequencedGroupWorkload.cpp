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

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace robotick
{
	struct SequencedGroupWorkloadImpl
	{
		std::vector<const WorkloadInstanceInfo*> children;

		void set_children(const std::vector<const WorkloadInstanceInfo*>& child_workloads) { children = child_workloads; }

		void start(double) { /* nothing needed */ }

		void tick(double time_delta)
		{
			auto start_time = std::chrono::steady_clock::now();

			for (auto* child : children)
			{
				assert(child);
				if (child != nullptr && child->type->tick_fn != nullptr)
				{
					child->type->tick_fn(child->ptr, time_delta);
				}
			}

			auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start_time).count();

			if (elapsed > time_delta)
			{
				std::printf("[Sequenced] Overrun: tick took %.3fms (budget %.3fms)\n", elapsed * 1000.0, time_delta * 1000.0);
			}
		}

		void stop() {} // nothing to do
	};

	struct SequencedGroupWorkload
	{
		SequencedGroupWorkloadImpl* impl = nullptr;

		SequencedGroupWorkload() : impl(new SequencedGroupWorkloadImpl()) {}
		~SequencedGroupWorkload()
		{
			stop();
			delete impl;
		}

		void set_children(const std::vector<const WorkloadInstanceInfo*>& children) { impl->set_children(children); }

		void start(double tick_rate_hz) { impl->start(tick_rate_hz); }

		void tick(double time_delta) { impl->tick(time_delta); }

		void stop() { impl->stop(); }
	};

	static WorkloadAutoRegister<SequencedGroupWorkload> s_auto_register;

} // namespace robotick
