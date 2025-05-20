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

#pragma once

#include <any>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace robotick
{
	struct WorkloadHandle
	{
		uint32_t index = uint32_t(-1); // Indicates invalid/unset by default
		bool is_valid() const { return index != uint32_t(-1); }
	};

	class Model
	{
	  public:
		static constexpr double TICK_RATE_FROM_PARENT = 0.0;

		struct WorkloadSeed
		{
			std::string type;
			std::string name;
			double tick_rate_hz;
			std::vector<WorkloadHandle> children;
			std::map<std::string, std::any> config;
		};

		WorkloadHandle add(const std::string& type, const std::string& name, double tick_rate_hz = TICK_RATE_FROM_PARENT,
			const std::map<std::string, std::any>& config = {})
		{
			assert(!m_root.is_valid() && "Model root must be added last");
			const std::vector<WorkloadHandle> children = {};
			m_configs.push_back({type, name, tick_rate_hz, children, config});
			return {static_cast<uint32_t>(m_configs.size() - 1)};
		}

		WorkloadHandle add(const std::string& type, const std::string& name, const std::vector<WorkloadHandle>& children,
			double tick_rate_hz = TICK_RATE_FROM_PARENT, const std::map<std::string, std::any>& config = {})
		{
			assert(!m_root.is_valid() && "Model root must be added last");
			m_configs.push_back({type, name, tick_rate_hz, children, config});
			return {static_cast<uint32_t>(m_configs.size() - 1)};
		}

		const std::vector<WorkloadSeed>& get_workload_seeds() const { return m_configs; }

		void set_root(WorkloadHandle handle, const bool auto_finalize = true)
		{
			m_root = handle; // no more changes once root has been set, so good time to validate the model...
			if (auto_finalize)
			{
				finalize();
			}
		}

		WorkloadHandle get_root() const { return m_root; }

		void finalize();

	  private:
		std::vector<WorkloadSeed> m_configs;
		WorkloadHandle m_root; // <- Root workload entry point
	};
} // namespace robotick
