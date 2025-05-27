// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/PyBind.h"
#include "robotick/framework/utils/PythonRuntime.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;
namespace robotick
{

	// === Field registrations ===

	struct PythonConfig
	{
		FixedString128 script_name;
		FixedString64 class_name;
	};
	ROBOTICK_BEGIN_FIELDS(PythonConfig)
	ROBOTICK_FIELD(PythonConfig, script_name)
	ROBOTICK_FIELD(PythonConfig, class_name)
	ROBOTICK_END_FIELDS()

	struct PythonInputs
	{
		double example_in = 0.0;
	};
	ROBOTICK_BEGIN_FIELDS(PythonInputs)
	ROBOTICK_FIELD(PythonInputs, example_in)
	ROBOTICK_END_FIELDS()

	struct PythonOutputs
	{
		double example_out = 0.0;
	};
	ROBOTICK_BEGIN_FIELDS(PythonOutputs)
	ROBOTICK_FIELD(PythonOutputs, example_out)
	ROBOTICK_END_FIELDS()

	// === Internal state (not reflected) ===

	struct __attribute__((visibility("hidden"))) PythonInternalState
	{
		py::object py_module;
		py::object py_class;
		py::object py_instance;
	};

	// === Workload ===

	struct __attribute__((visibility("hidden"))) PythonWorkload
	{
		PythonConfig config;
		PythonInputs inputs;
		PythonOutputs outputs;

		// PythonInternalState is heap-allocated to keep PythonWorkload standard-layout. This enables
		// reflection, runtime construction in raw memory, and predictable data layout for logging and interop.
		std::unique_ptr<PythonInternalState> internal_state = nullptr;

		PythonWorkload() : internal_state(std::make_unique<PythonInternalState>()) {}

		~PythonWorkload()
		{
			py::gil_scoped_acquire gil;
			internal_state.reset(); // clear (and thus delete) explicitly while we have gil lock
		}

		PythonWorkload(const PythonWorkload&) = delete;
		PythonWorkload& operator=(const PythonWorkload&) = delete;
		PythonWorkload(PythonWorkload&&) noexcept = default;
		PythonWorkload& operator=(PythonWorkload&&) noexcept = default;

		void load()
		{
			robotick::ensure_python_runtime(); // ensures it's alive

			try
			{
				py::gil_scoped_acquire gil;

				py::dict py_cfg;
				const auto* info = get_struct_reflection<PythonConfig>();
				assert(info != nullptr && "Struct not registered: PythonConfig");
				marshal_struct_to_dict(&config, *info, py_cfg);

				internal_state->py_module = py::module_::import(config.script_name.c_str());
				internal_state->py_class = internal_state->py_module.attr(config.class_name.c_str());
				internal_state->py_instance = internal_state->py_class(py_cfg);
			}
			catch (const py::error_already_set& e)
			{
				std::cerr << "[Python ERROR] Failed to load workload: " << e.what() << std::endl;
				throw;
			}
			catch (const std::exception& e)
			{
				std::cerr << "[ERROR] Exception during Python workload load(): " << e.what() << std::endl;
				throw;
			}
		}

		void tick(double time_delta)
		{
			if (!internal_state->py_instance)
				return;

			py::gil_scoped_acquire gil;

			py::dict py_in;
			const auto* input_info = get_struct_reflection<PythonInputs>();
			assert(input_info && "Struct not registered: PythonInputs");
			marshal_struct_to_dict(&inputs, *input_info, py_in);

			try
			{
				py::dict py_out;
				internal_state->py_instance.attr("tick")(time_delta, py_in, py_out);

				const auto* output_info = get_struct_reflection<PythonOutputs>();
				assert(output_info && "Struct not registered: PythonOutputs");
				marshal_dict_to_struct(py_out, *output_info, &outputs);
			}
			catch (const py::error_already_set& e)
			{
				std::cerr << "[Python ERROR] " << e.what() << std::endl;
			}
		}
	};

	// === Auto-registration ===

	ROBOTICK_DEFINE_WORKLOAD(PythonWorkload)

} // namespace robotick