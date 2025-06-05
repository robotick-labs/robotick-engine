// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"

namespace robotick
{

	struct BlackboardTestUtils
	{
		static std::pair<WorkloadsBuffer, Blackboard*> make_buffer_and_embedded_blackboard(const std::vector<BlackboardFieldInfo>& schema)
		{
			// create a temp-blackboard on the stack to find out how much data-block space the scheme needs
			Blackboard temp(schema);
			size_t total_size = sizeof(Blackboard) + temp.get_info()->total_datablock_size;

			// create a sufficiently large WorkloadsBuffer for actual Blackboard and its data
			WorkloadsBuffer buffer(total_size);

			// create the blackboard:
			auto* blackboard_ptr = buffer.as<Blackboard>(0);
			new (blackboard_ptr) Blackboard(schema);
			blackboard_ptr->bind(sizeof(Blackboard));
			return {std::move(buffer), blackboard_ptr};
		}

		static const BlackboardInfo& get_info(Blackboard& blackboard) { return *blackboard.get_info(); }

		static const BlackboardFieldInfo* get_field_info(const Blackboard& blackboard, const std::string& key)
		{
			return blackboard.get_field_info(key);
		};
	};

} // namespace robotick