// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <stdexcept>
#include <string>
#include <typeinfo>

namespace robotick::test
{
	struct EngineInspector
	{
		static const WorkloadInstanceInfo& get_instance_info(const Engine& engine, size_t index) { return engine.get_instance_info(index); }

		static const std::vector<WorkloadInstanceInfo>& get_all_instance_info(const Engine& engine) { return engine.get_all_instance_info(); }

		template <typename T> static T* get_instance(const Engine& engine, size_t index)
		{
			const WorkloadInstanceInfo& info = get_instance_info(engine, index);
			return static_cast<T*>((void*)info.get_ptr(engine));
		}

		static const std::vector<DataConnectionInfo>& get_all_data_connections(const Engine& engine) { return engine.get_all_data_connections(); }

		static WorkloadsBuffer& get_workloads_buffer(const Engine& engine) { return engine.get_workloads_buffer(); }
	};
} // namespace robotick::test