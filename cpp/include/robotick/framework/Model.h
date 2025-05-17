// Model.h
#pragma once

#include <any>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace robotick
{
	struct WorkloadHandle
	{
		uint32_t index;
	};

	class Model
	{
	  public:
		struct WorkloadSeed
		{
			std::string						type;
			std::string						name;
			double							tick_rate_hz;
			std::map<std::string, std::any> config;
		};

		WorkloadHandle add(const std::string& type, const std::string& name, double tick_rate_hz,
						   const std::map<std::string, std::any>& config)
		{
			m_configs.push_back({type, name, tick_rate_hz, config});
			return {static_cast<uint32_t>(m_configs.size() - 1)};
		}

		const std::vector<WorkloadSeed>& get_workload_seeds() const { return m_configs; }

	  private:
		std::vector<WorkloadSeed> m_configs;
	};
} // namespace robotick
