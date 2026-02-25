// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"

namespace robotick
{

	struct BlackboardBuffer
	{
		BlackboardBuffer() = default;
		BlackboardBuffer(const BlackboardBuffer&) = delete;
		BlackboardBuffer& operator=(const BlackboardBuffer&) = delete;
		BlackboardBuffer(BlackboardBuffer&&) = default;
		BlackboardBuffer& operator=(BlackboardBuffer&&) = default;

		WorkloadsBuffer buffer;
		Blackboard* blackboard = nullptr;
	};

	struct BlackboardTestUtils
	{
		static BlackboardBuffer make_buffer_and_embedded_blackboard(const HeapVector<FieldDescriptor>& fields)
		{
			// create a temp-blackboard on the stack to find out how much data-block space the scheme needs
			Blackboard temp_blackboard;
			temp_blackboard.initialize_fields(fields);

			const size_t total_size = sizeof(Blackboard) + temp_blackboard.get_info().total_datablock_size;

			BlackboardBuffer result{WorkloadsBuffer(total_size), nullptr};

			// create the blackboard:
			Blackboard* blackboard_ptr = result.buffer.as<Blackboard>(0);
			new (blackboard_ptr) Blackboard();
			blackboard_ptr->initialize_fields(fields);

			size_t datablock_offset = sizeof(Blackboard);
			blackboard_ptr->bind(result.buffer, datablock_offset);

			ROBOTICK_ASSERT_MSG(datablock_offset == total_size, "Datablock-offset pointer should have been incremented ready for next blackboard");

			result.blackboard = blackboard_ptr;
			return result;
		}
	};

} // namespace robotick
