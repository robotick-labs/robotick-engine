// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <pybind11/embed.h>

namespace robotick
{
	std::shared_ptr<pybind11::scoped_interpreter>& get_python_interpreter_singleton()
	{
		static std::shared_ptr<pybind11::scoped_interpreter> instance;
		return instance;
	}

	void ensure_python_runtime()
	{
		// This lambda runs exactly once, on first call.
		// It initializes the interpreter (and acquires the GIL),
		// then immediately saves/releases the thread state,
		// so that future gil_scoped_acquire() calls will work.
		static std::shared_ptr<pybind11::scoped_interpreter> guard = []()
		{
			auto g = std::make_shared<pybind11::scoped_interpreter>();
			PyEval_SaveThread(); // release the GIL
			return g;
		}();
		(void)guard; // silence unused-variable warnings
	}
} // namespace robotick
