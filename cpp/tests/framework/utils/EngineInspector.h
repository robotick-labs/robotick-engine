// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Engine.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/Typename.h"

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
			const std::string expected_type = get_clean_typename(typeid(T));

			if (info.type->name != expected_type)
			{
				throw std::runtime_error("Type mismatch: expected " + expected_type + ", got " + info.type->name);
			}

			return static_cast<T*>((void*)info.ptr);
		}

		static const std::vector<DataConnectionInfo>& get_all_data_connections(const Engine& engine) { return engine.get_all_data_connections(); }

		static const WorkloadsBuffer& get_workloads_buffer_readonly(const Engine& engine) { return engine.get_workloads_buffer_readonly(); }
	};
} // namespace robotick::test