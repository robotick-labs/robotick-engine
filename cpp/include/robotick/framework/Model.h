#pragma once

#include "robotick/framework/registry/WorkloadFactory.h"
#include <any>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace robotick
{

	class Model
	{
	  public:
		WorkloadHandle add_by_type(const std::string &type, const std::string &name, const double tick_rate_hz,
								   const std::map<std::string, std::any> &config)
		{
			auto handle = m_factory.add_by_type(type, name, tick_rate_hz, config);
			m_handles.push_back(handle);
			return handle;
		}

		void finalise()
		{
			m_factory.finalise();
		}

		const std::vector<WorkloadHandle> &get_workloads() const
		{
			return m_handles;
		}

		WorkloadFactory &factory()
		{
			return m_factory;
		}
		const WorkloadFactory &factory() const
		{
			return m_factory;
		}

		const WorkloadInstance &get_instance(WorkloadHandle h) const
		{
			return m_factory.get_all().at(h.index);
		}

		template <typename T> T *get(WorkloadHandle h)
		{
			return static_cast<T *>(m_factory.get_all().at(h.index).ptr);
		}

		template <typename T> const T *get(WorkloadHandle h) const
		{
			return static_cast<const T *>(m_factory.get_all().at(h.index).ptr);
		}

	  private:
		WorkloadFactory m_factory;
		std::vector<WorkloadHandle> m_handles;
	};

} // namespace robotick
