// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/api.h"

#include <atomic>
#include <memory>
#include <string>

namespace robotick
{
	struct WorkloadRegistryEntry;

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

		// The stop_flag must outlive this call. Do not pass temporaries.
		void run(const std::atomic<bool>& stop_after_next_tick_flag);

		void run(const std::atomic<bool>&&) = delete; // cause compile-error if a temporary is used

	  protected:
		const WorkloadInstanceInfo& get_instance_info(size_t index) const;
		const std::vector<WorkloadInstanceInfo>& get_all_instance_info() const;

	  private:
		ROBOTICK_DECLARE_PIMPL();
	};
} // namespace robotick
