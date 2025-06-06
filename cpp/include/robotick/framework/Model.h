// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/DataConnection.h"

#include <algorithm>
#include <any>
#include <cassert>
#include <cstdint>
#include <map>
#include <stdexcept>
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
			if (root_workload.is_valid())
				throw std::logic_error("Cannot add workloads after root has been set. Model root must be set last.");

			const std::vector<WorkloadHandle> children = {};
			workload_seeds.push_back({type, name, tick_rate_hz, children, config});
			return {static_cast<uint32_t>(workload_seeds.size() - 1)};
		}

		WorkloadHandle add(const std::string& type, const std::string& name, const std::vector<WorkloadHandle>& children,
			double tick_rate_hz = TICK_RATE_FROM_PARENT, const std::map<std::string, std::any>& config = {})
		{
			if (root_workload.is_valid())
				throw std::logic_error("Cannot add workloads after root has been set. Model root must be set last.");

			for (auto child : children)
			{
				if (!child.is_valid() || child.index >= workload_seeds.size())
					throw std::out_of_range("Child handle out of range when adding workload '" + name + "'");
			}

			workload_seeds.push_back({type, name, tick_rate_hz, children, config});
			return {static_cast<uint32_t>(workload_seeds.size() - 1)};
		}

		void connect(const std::string& source_field_path, const std::string& dest_field_path);

		void set_root(WorkloadHandle handle, const bool auto_finalize = true)
		{
			root_workload = handle; // no more changes once root has been set, so good time to validate the model...
			if (auto_finalize)
			{
				finalize();
			}
		}

		const std::vector<WorkloadSeed>& get_workload_seeds() const { return workload_seeds; }

		const std::vector<DataConnectionSeed>& get_data_connection_seeds() const { return data_connection_seeds; }

		WorkloadHandle get_root() const { return root_workload; }

		void finalize();

	  private:
		std::vector<WorkloadSeed> workload_seeds;
		std::vector<DataConnectionSeed> data_connection_seeds;
		WorkloadHandle root_workload; // <- Root workload entry point
	};
} // namespace robotick
