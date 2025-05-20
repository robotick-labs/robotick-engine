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

#include "robotick/framework/Engine.h"
#include "robotick/framework/utils/Typename.h"

#include <stdexcept>
#include <string>
#include <typeinfo>

namespace robotick::test
{
	struct EngineInspector
	{
		static const WorkloadInstanceInfo& get_instance_info(const Engine& engine, size_t index) { return engine.get_instance_info(index); }

		template <typename T> static const T* get_instance(const Engine& engine, size_t index)
		{
			const WorkloadInstanceInfo& info = get_instance_info(engine, index);
			const std::string expected_type = get_clean_typename(typeid(T));

			if (info.type->name != expected_type)
			{
				throw std::runtime_error("Type mismatch: expected " + expected_type + ", got " + info.type->name);
			}

			return static_cast<const T*>(info.ptr);
		}
	};
} // namespace robotick::test