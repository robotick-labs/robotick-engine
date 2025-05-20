// Copyright 2025 Robotick Labs
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "robotick/framework/FixedString.h"
#include "robotick/framework/registry/FieldMacros.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/PyBind.h"

#include <iostream>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace robotick
{
	namespace
	{
		// === Structs ===

		struct PythonConfig
		{
			FixedString128 script_name;
			FixedString64 class_name;
			ROBOTICK_DECLARE_FIELDS();
		};
		ROBOTICK_DEFINE_FIELDS(PythonConfig, ROBOTICK_FIELD(PythonConfig, script_name), ROBOTICK_FIELD(PythonConfig, class_name))

		struct PythonInputs
		{
			double example_in = 0.0;
			ROBOTICK_DECLARE_FIELDS();
		};
		ROBOTICK_DEFINE_FIELDS(PythonInputs, ROBOTICK_FIELD(PythonInputs, example_in))

		struct PythonOutputs
		{
			double example_out = 0.0;
			ROBOTICK_DECLARE_FIELDS();
		};
		ROBOTICK_DEFINE_FIELDS(PythonOutputs, ROBOTICK_FIELD(PythonOutputs, example_out))

		struct PythonInternalState
		{
			// Python embedding members
			py::object py_module;
			py::object py_class;
			py::object py_instance;
		};
	}; // namespace

	// === Workload ===

	struct PythonWorkload
	{
		PythonConfig config;
		PythonInputs inputs;
		PythonOutputs outputs;

		PythonInternalState* internal_state = nullptr;

		PythonWorkload() { internal_state = new PythonInternalState(); }

		~PythonWorkload()
		{
			py::gil_scoped_acquire gil;
			delete internal_state;
		}

		void load()
		{
			static std::once_flag init_flag;
			std::call_once(init_flag,
				[]()
				{
					py::initialize_interpreter();
					PyEval_SaveThread(); // release the GIL
				});

			try
			{
				py::gil_scoped_acquire gil;

				py::dict py_cfg;
				marshal_struct_to_dict(&config, *config.get_struct_reflection(), py_cfg);

				internal_state->py_module = py::module_::import(config.script_name.c_str());
				internal_state->py_class = internal_state->py_module.attr(config.class_name.c_str());
				internal_state->py_instance = internal_state->py_class(py_cfg);
			}
			catch (const py::error_already_set& e)
			{
				std::cerr << "[Python ERROR] Failed to load workload: " << e.what() << std::endl;
				throw; // rethrow same exception
			}
			catch (const std::exception& e)
			{
				std::cerr << "[ERROR] Exception during Python workload load(): " << e.what() << std::endl;
				throw; // rethrow same exception
			}
		}

		void tick(double time_delta)
		{
			if (!internal_state->py_instance)
				return;

			// Acquire GIL for calling into Python
			py::gil_scoped_acquire gil;

			// Prepare inputs
			py::dict py_in;
			marshal_struct_to_dict(&inputs, *inputs.get_struct_reflection(), py_in);
			py::dict py_out;

			try
			{
				// Call the Python 'tick' method
				internal_state->py_instance.attr("tick")(time_delta, py_in, py_out);
				// Marshal outputs back into C++ struct
				marshal_dict_to_struct(py_out, *outputs.get_struct_reflection(), &outputs);
			}
			catch (const py::error_already_set& e)
			{
				std::cerr << "[Python ERROR] " << e.what() << std::endl;
			}
		}
	};

	static robotick::WorkloadAutoRegister<PythonWorkload, PythonConfig, PythonInputs, PythonOutputs> s_auto_register;

} // namespace robotick