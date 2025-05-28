// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Buffer.h"

namespace robotick
{
	// BlackboardsBuffer static members
	thread_local BlackboardsBuffer BlackboardsBuffer::local_instance;
	BlackboardsBuffer* BlackboardsBuffer::source_buffer = nullptr;

	BlackboardsBuffer& BlackboardsBuffer::get_local_mirror()
	{
		return local_instance;
	}

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

	void BlackboardsBuffer::mirror_from_source()
	{
		if (!source_buffer)
			throw std::runtime_error("BlackboardsBuffer::mirror_from_source: no source set");
		local_instance.mirror_from(*source_buffer);
	}

	// WorkloadsBuffer static members
	thread_local WorkloadsBuffer WorkloadsBuffer::local_instance;
	WorkloadsBuffer* WorkloadsBuffer::source_buffer = nullptr;

	WorkloadsBuffer& WorkloadsBuffer::get_local_mirror()
	{
		return local_instance;
	}

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

	void WorkloadsBuffer::mirror_from_source()
	{
		if (!source_buffer)
			throw std::runtime_error("WorkloadsBuffer::mirror_from_source: no source set");
		local_instance.mirror_from(*source_buffer);
	}
} // namespace robotick
