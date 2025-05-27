// Copyright 2025 Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include <pybind11/embed.h>

namespace robotick
{

	// On first call, allocate a scoped_interpreter and never delete it.
	// That way Python is initialized exactly once, we release the GIL for future use,
	// and we never try to finalize after things get torn down.
	void ensure_python_runtime()
	{
		static auto* guard = []()
		{
			auto* g = new pybind11::scoped_interpreter{};
			PyEval_SaveThread(); // drop the GIL so gil_scoped_acquire works later
			return g;
		}();
		(void)guard;
	}

} // namespace robotick
