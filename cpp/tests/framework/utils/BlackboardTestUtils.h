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
		static std::pair<WorkloadsBuffer, Blackboard*> make_buffer_and_embedded_blackboard(const HeapVector<FieldDescriptor>& fields)
		{
			// create a temp-blackboard on the stack to find out how much data-block space the scheme needs
			Blackboard temp_blackboard;
			temp_blackboard.initialize_fields(fields);

			const size_t total_size = sizeof(Blackboard) + temp_blackboard.get_info().total_datablock_size;

			// create a sufficiently large WorkloadsBuffer for actual Blackboard and its data
			WorkloadsBuffer buffer(total_size);

			// create the blackboard:
			Blackboard* blackboard_ptr = buffer.as<Blackboard>(0);
			new (blackboard_ptr) Blackboard();
			blackboard_ptr->initialize_fields(fields);
#
			size_t datablock_offset = sizeof(Blackboard);
			blackboard_ptr->bind(datablock_offset);

			ROBOTICK_ASSERT_MSG(datablock_offset == total_size, "Datablock-offset pointer should have been incremented ready for next blackboard");

			return {std::move(buffer), blackboard_ptr};
		}
	};

} // namespace robotick