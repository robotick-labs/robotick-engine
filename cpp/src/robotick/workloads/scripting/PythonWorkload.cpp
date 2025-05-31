// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/PythonRuntime.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace robotick
{

	struct PythonConfig
	{
		FixedString128 script_name;
		FixedString64 class_name;
		Blackboard blackboard;
	};
	ROBOTICK_BEGIN_FIELDS(PythonConfig)
	ROBOTICK_FIELD(PythonConfig, script_name)
	ROBOTICK_FIELD(PythonConfig, class_name)
	ROBOTICK_FIELD(PythonConfig, blackboard)
	ROBOTICK_END_FIELDS()

	struct PythonInputs
	{
		Blackboard blackboard;
	};
	ROBOTICK_BEGIN_FIELDS(PythonInputs)
	ROBOTICK_FIELD(PythonInputs, blackboard)
	ROBOTICK_END_FIELDS()

	struct PythonOutputs
	{
		Blackboard blackboard;
	};
	ROBOTICK_BEGIN_FIELDS(PythonOutputs)
	ROBOTICK_FIELD(PythonOutputs, blackboard)
	ROBOTICK_END_FIELDS()

	struct __attribute__((visibility("hidden"))) PythonInternalState
	{
		py::object py_module;
		py::object py_class;
		py::object py_instance;
	};

	struct __attribute__((visibility("hidden"))) PythonWorkload
	{
		PythonConfig config;
		PythonInputs inputs;
		PythonOutputs outputs;

		std::unique_ptr<PythonInternalState> internal_state;

		PythonWorkload() : internal_state(std::make_unique<PythonInternalState>()) {}

		~PythonWorkload()
		{
			py::gil_scoped_acquire gil;
			internal_state.reset();
		}

		PythonWorkload(const PythonWorkload&) = delete;
		PythonWorkload& operator=(const PythonWorkload&) = delete;
		PythonWorkload(PythonWorkload&&) noexcept = default;
		PythonWorkload& operator=(PythonWorkload&&) noexcept = default;

		std::vector<BlackboardFieldInfo> parse_blackboard_schema(const py::dict& desc_dict)
		{
			std::vector<BlackboardFieldInfo> fields;

			for (auto item : desc_dict)
			{
				std::string name = py::str(item.first);
				std::string type_str = py::str(item.second);
				std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

				std::type_index type(typeid(void));

				if (type_str == "int")
					type = std::type_index(typeid(int));
				else if (type_str == "double")
					type = std::type_index(typeid(double));
				else if (type_str == "fixedstring64")
					type = std::type_index(typeid(FixedString64));
				else if (type_str == "fixedstring128")
					type = std::type_index(typeid(FixedString128));
				else
					ROBOTICK_ERROR("Unsupported field type: %s", type_str.c_str());

				fields.emplace_back(FixedString64(name.c_str()), type);
			}

			return fields;
		}

		void initialize_blackboards(py::object& py_class)
		{
			py::dict desc = py_class.attr("describe")();
			config.blackboard = Blackboard(parse_blackboard_schema(desc["config"]));
			inputs.blackboard = Blackboard(parse_blackboard_schema(desc["inputs"]));
			outputs.blackboard = Blackboard(parse_blackboard_schema(desc["outputs"]));
		}

		void pre_load()
		{
			if (config.script_name.empty() || config.class_name.empty())
				ROBOTICK_ERROR("PythonWorkload config must specify script_name and class_name");

			robotick::ensure_python_runtime();
			py::gil_scoped_acquire gil;

			internal_state->py_module = py::module_::import(config.script_name.c_str());
			internal_state->py_class = internal_state->py_module.attr(config.class_name.c_str());

			initialize_blackboards(internal_state->py_class);
		}

		void load()
		{
			robotick::ensure_python_runtime();
			py::gil_scoped_acquire gil;

			py::dict py_cfg;
			for (const auto& field : config.blackboard.get_schema())
			{
				const std::string key = field.name.c_str();
				const auto& type = field.type;

				if (type == typeid(int))
					py_cfg[key.c_str()] = config.blackboard.get<int>(key);
				else if (type == typeid(double))
					py_cfg[key.c_str()] = config.blackboard.get<double>(key);
				else if (type == typeid(FixedString64))
					py_cfg[key.c_str()] = std::string(config.blackboard.get<FixedString64>(key).c_str());
				else if (type == typeid(FixedString128))
					py_cfg[key.c_str()] = std::string(config.blackboard.get<FixedString128>(key).c_str());
				else
					ROBOTICK_ERROR("Unsupported config field type for key '%s' in PythonWorkload", key.c_str());
			}

			internal_state->py_instance = internal_state->py_class(py_cfg);
		}

		void tick(double time_delta)
		{
			if (!internal_state->py_instance)
				return;

			py::gil_scoped_acquire gil;

			py::dict py_in;
			for (const auto& field : inputs.blackboard.get_schema())
			{
				const std::string key = field.name.c_str();
				const auto& type = field.type;

				if (type == typeid(int))
					py_in[key.c_str()] = inputs.blackboard.get<int>(key);
				else if (type == typeid(double))
					py_in[key.c_str()] = inputs.blackboard.get<double>(key);
				else if (type == typeid(FixedString64))
					py_in[key.c_str()] = std::string(inputs.blackboard.get<FixedString64>(key).c_str());
				else if (type == typeid(FixedString128))
					py_in[key.c_str()] = std::string(inputs.blackboard.get<FixedString128>(key).c_str());
			}

			py::dict py_out;
			internal_state->py_instance.attr("tick")(time_delta, py_in, py_out);

			for (auto item : py_out)
			{
				std::string key = py::str(item.first);
				auto val = item.second;

				const auto& schema = outputs.blackboard.get_schema();
				auto it = std::find_if(schema.begin(), schema.end(),
					[&](const BlackboardFieldInfo& f)
					{
						return key == f.name.c_str();
					});
				if (it == schema.end())
					continue;

				if (it->type == typeid(int))
					outputs.blackboard.set<int>(key, val.cast<int>());
				else if (it->type == typeid(double))
					outputs.blackboard.set<double>(key, val.cast<double>());
				else if (it->type == typeid(FixedString64))
					outputs.blackboard.set<FixedString64>(key, FixedString64(val.cast<std::string>().c_str()));
				else if (it->type == typeid(FixedString128))
					outputs.blackboard.set<FixedString128>(key, FixedString128(val.cast<std::string>().c_str()));
			}
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(PythonWorkload)

} // namespace robotick
