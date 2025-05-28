// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Buffer.h"

namespace robotick
{
	// ---- BlackboardsBuffer ----

	thread_local const BlackboardsBuffer* BlackboardsBuffer::source_buffer = nullptr;

	BlackboardsBuffer& BlackboardsBuffer::get()
	{
		thread_local BlackboardsBuffer buffer;
		return buffer;
	}

	void BlackboardsBuffer::set_source(const BlackboardsBuffer* buffer)
	{
		source_buffer = buffer;
	}

	void BlackboardsBuffer::mirror_from_source()
	{
		if (!source_buffer)
			throw std::runtime_error("BlackboardsBuffer: no source buffer set");
		this->mirror_from(*source_buffer);
	}

	// ---- WorkloadsBuffer ----

	thread_local const WorkloadsBuffer* WorkloadsBuffer::source_buffer = nullptr;

	WorkloadsBuffer& WorkloadsBuffer::get()
	{
		thread_local WorkloadsBuffer buffer;
		return buffer;
	}

	void WorkloadsBuffer::set_source(const WorkloadsBuffer* buffer)
	{
		source_buffer = buffer;
	}

	void WorkloadsBuffer::mirror_from_source()
	{
		if (!source_buffer)
			throw std::runtime_error("WorkloadsBuffer: no source buffer set");
		this->mirror_from(*source_buffer);
	}

} // namespace robotick
