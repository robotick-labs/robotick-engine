// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Buffer.h"
#include <stdexcept>

namespace robotick
{
	// BlackboardsBuffer static members
	BlackboardsBuffer* BlackboardsBuffer::source_buffer = nullptr;

	BlackboardsBuffer& BlackboardsBuffer::get_source()
	{
		if (!source_buffer)
			throw std::runtime_error("BlackboardsBuffer::get_source: no source set");
		return *source_buffer;
	}

	void BlackboardsBuffer::set_source(BlackboardsBuffer* buffer)
	{
		source_buffer = buffer;
	}

	// WorkloadsBuffer static members
	WorkloadsBuffer* WorkloadsBuffer::source_buffer = nullptr;

	WorkloadsBuffer& WorkloadsBuffer::get_source()
	{
		if (!source_buffer)
			throw std::runtime_error("WorkloadsBuffer::get_source: no source set");
		return *source_buffer;
	}

	void WorkloadsBuffer::set_source(WorkloadsBuffer* buffer)
	{
		source_buffer = buffer;
	}
} // namespace robotick
