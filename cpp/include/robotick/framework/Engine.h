// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

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
