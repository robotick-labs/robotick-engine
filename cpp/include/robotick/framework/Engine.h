// Engine.h
#pragma once

#include "robotick/framework/api.h"

#include <memory>
#include <string>

namespace robotick
{
	class Model;
	struct WorkloadRegistryEntry;

	struct WorkloadInstanceInfo
	{
		void*						 ptr;
		const WorkloadRegistryEntry* type;
		std::string					 unique_name;
		double						 tick_rate_hz;
	};

	namespace test_access
	{
		struct EngineInspector;
	}

	class ROBOTICK_API Engine
	{
		friend struct robotick::test_access::EngineInspector;

	  public:
		Engine();
		~Engine();

		void load(const Model& model);
		void start();
		void stop();

	  protected:
		const WorkloadInstanceInfo& get_instance_info(size_t index) const;

	  private:
		ROBOTICK_DECLARE_PIMPL();
	};
} // namespace robotick