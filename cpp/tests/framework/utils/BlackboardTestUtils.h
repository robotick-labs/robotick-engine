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
		static size_t align_offset(size_t offset, size_t alignment)
		{
			ROBOTICK_ASSERT_MSG(alignment > 0, "BlackboardTestUtils::align_offset() requires alignment > 0");
			const size_t remainder = offset % alignment;
			return (remainder == 0) ? offset : (offset + (alignment - remainder));
		}

		static BlackboardBuffer make_buffer_and_embedded_blackboard(const HeapVector<FieldDescriptor>& fields)
		{
			// create a temp-blackboard on the stack to find out how much data-block space the scheme needs
			Blackboard temp_blackboard;
			temp_blackboard.initialize_fields(fields);

			DynamicStructStoragePlan storage_plan;
			ROBOTICK_ASSERT(Blackboard::plan_storage(&temp_blackboard, storage_plan));

			const size_t datablock_offset = align_offset(sizeof(Blackboard), storage_plan.alignment);
			const size_t total_size = datablock_offset + storage_plan.size_bytes;

			BlackboardBuffer result{WorkloadsBuffer(total_size), nullptr};

			// create the blackboard:
			Blackboard* blackboard_ptr = result.buffer.as<Blackboard>(0);
			new (blackboard_ptr) Blackboard();
			blackboard_ptr->initialize_fields(fields);

			ROBOTICK_ASSERT(Blackboard::bind_storage(blackboard_ptr, result.buffer, datablock_offset, storage_plan.size_bytes));

			size_t expected_end = datablock_offset + storage_plan.size_bytes;
			ROBOTICK_ASSERT_MSG(expected_end == total_size, "Datablock-offset pointer should have been incremented ready for next blackboard");

			result.blackboard = blackboard_ptr;
			return result;
		}
	};

} // namespace robotick
