// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"
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
	ROBOTICK_REGISTER_STRUCT_BEGIN(PythonConfig)
	ROBOTICK_STRUCT_FIELD(PythonConfig, FixedString128, script_name)
	ROBOTICK_STRUCT_FIELD(PythonConfig, FixedString64, class_name)
	ROBOTICK_STRUCT_FIELD(PythonConfig, Blackboard, blackboard)
	ROBOTICK_REGISTER_STRUCT_END(PythonConfig)

	struct PythonInputs
	{
		Blackboard blackboard;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(PythonInputs)
	ROBOTICK_STRUCT_FIELD(PythonInputs, Blackboard, blackboard)
	ROBOTICK_REGISTER_STRUCT_END(PythonInputs)

	struct PythonOutputs
	{
		Blackboard blackboard;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(PythonOutputs)
	ROBOTICK_STRUCT_FIELD(PythonOutputs, Blackboard, blackboard)
	ROBOTICK_REGISTER_STRUCT_END(PythonOutputs)

	struct __attribute__((visibility("hidden"))) PythonInternalState
	{
		py::object py_module;
		py::object py_class;
		py::object py_instance;

		HeapVector<FieldDescriptor> config_fields;
		HeapVector<FieldDescriptor> input_fields;
		HeapVector<FieldDescriptor> output_fields;
	};

	struct __attribute__((visibility("hidden"))) PythonWorkload
	{
		PythonConfig config;
		PythonInputs inputs;
		PythonOutputs outputs;

		std::unique_ptr<PythonInternalState> internal_state;

		PythonWorkload()
			: internal_state(std::make_unique<PythonInternalState>())
		{
		}

		~PythonWorkload()
		{
			py::gil_scoped_acquire gil;
			internal_state.reset();
		}

		PythonWorkload(const PythonWorkload&) = delete;
		PythonWorkload& operator=(const PythonWorkload&) = delete;
		PythonWorkload(PythonWorkload&&) noexcept = default;
		PythonWorkload& operator=(PythonWorkload&&) noexcept = default;

		void parse_blackboard_schema(const py::dict& desc_dict, HeapVector<FieldDescriptor>& fields)
		{
			size_t field_offset = 0;
			size_t field_index = 0;
			fields.initialize(desc_dict.size());

			for (auto item : desc_dict)
			{
				FieldDescriptor& field_desc = fields[field_index];

				// Extract field name
				const std::string name_str = py::str(item.first).cast<std::string>();
				field_desc.name = name_str.c_str(); // safe: copied into FixedString

				// Extract and parse type string
				const std::string type_str_raw = py::str(item.second).cast<std::string>();
				const FixedString64 type_str = type_str_raw.c_str();

				// Set type_id based on known strings
				if (type_str == "int")
					field_desc.type_id = TypeId(GET_TYPE_ID(int));
				else if (type_str == "double")
					field_desc.type_id = TypeId(GET_TYPE_ID(double));
				else if (type_str == "fixedstring64")
					field_desc.type_id = TypeId(GET_TYPE_ID(FixedString64));
				else if (type_str == "fixedstring128")
					field_desc.type_id = TypeId(GET_TYPE_ID(FixedString128));
				else
					ROBOTICK_FATAL_EXIT("Unsupported field type: %s", type_str.c_str());

				// Resolve TypeDescriptor
				const TypeDescriptor* field_type = field_desc.find_type_descriptor();
				if (!field_type)
				{
					ROBOTICK_FATAL_EXIT(
						"Could not find type '%s' for Blackboard field: %s", field_desc.type_id.get_debug_name(), field_desc.name.c_str());
				}

				// Align field_offset based on required alignment
				const size_t align = field_type->alignment;
				field_offset = (field_offset + align - 1) & ~(align - 1);

				field_desc.offset = field_offset;
				field_offset += field_type->size;
				field_index++;
			}
		}

		void initialize_blackboards(py::object& py_class)
		{
			py::dict desc;

			// (note - we allow exceptions in PythonWorkload/Runtime only since Python libs require them - so the below is fine even with the wider
			// engine not supporting exceptions)
			try
			{
				desc = py_class.attr("describe")();
			}
			catch (const py::error_already_set& e)
			{
				ROBOTICK_FATAL_EXIT("Python class '%s' describe() failed: %s", config.class_name.c_str(), e.what());
			}

			parse_blackboard_schema(desc["config"], internal_state->config_fields);
			config.blackboard.initialize_fields(internal_state->config_fields);

			parse_blackboard_schema(desc["inputs"], internal_state->input_fields);
			inputs.blackboard.initialize_fields(internal_state->input_fields);

			parse_blackboard_schema(desc["outputs"], internal_state->output_fields);
			outputs.blackboard.initialize_fields(internal_state->output_fields);
		}

		void pre_load()
		{
			if (config.script_name.empty() || config.class_name.empty())
				ROBOTICK_FATAL_EXIT("PythonWorkload config must specify script_name and class_name");

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

			const StructDescriptor& struct_desc = config.blackboard.get_struct_descriptor();

			py::dict py_cfg;
			for (const auto& field : struct_desc.fields)
			{
				const std::string key = field.name.c_str();
				const auto& type = field.type_id;

				if (type == GET_TYPE_ID(int))
					py_cfg[key.c_str()] = config.blackboard.get<int>(key);
				else if (type == GET_TYPE_ID(double))
					py_cfg[key.c_str()] = config.blackboard.get<double>(key);
				else if (type == GET_TYPE_ID(FixedString64))
					py_cfg[key.c_str()] = std::string(config.blackboard.get<FixedString64>(key).c_str());
				else if (type == GET_TYPE_ID(FixedString128))
					py_cfg[key.c_str()] = std::string(config.blackboard.get<FixedString128>(key).c_str());
				else
					ROBOTICK_FATAL_EXIT("Unsupported config field type for key '%s' in PythonWorkload", key.c_str());
			}

			// (note - we allow exceptions in PythonWorkload/Runtime only since Python libs require them - so the below is fine even with the wider
			// engine not supporting exceptions)
			try
			{
				internal_state->py_instance = internal_state->py_class(py_cfg);
			}
			catch (const py::error_already_set& e)
			{
				ROBOTICK_FATAL_EXIT("Failed to instantiate Python class '%s': %s", config.class_name.c_str(), e.what());
			}
		}

		void tick(const TickInfo& tick_info)
		{
			if (!internal_state->py_instance)
				return;

			py::gil_scoped_acquire gil;

			py::dict py_in;
			py::dict py_out;

			const StructDescriptor& struct_desc = inputs.blackboard.get_struct_descriptor();

			for (const auto& field : struct_desc.fields)
			{
				const std::string key = field.name.c_str();
				const auto& type = field.type_id;

				if (type == GET_TYPE_ID(int))
					py_in[key.c_str()] = inputs.blackboard.get<int>(key);
				else if (type == GET_TYPE_ID(double))
					py_in[key.c_str()] = inputs.blackboard.get<double>(key);
				else if (type == GET_TYPE_ID(FixedString64))
					py_in[key.c_str()] = std::string(inputs.blackboard.get<FixedString64>(key).c_str());
				else if (type == GET_TYPE_ID(FixedString128))
					py_in[key.c_str()] = std::string(inputs.blackboard.get<FixedString128>(key).c_str());
			}

			// (note - we allow exceptions in PythonWorkload/Runtime only since Python libs require them - so the below is fine even with the wider
			// engine not supporting exceptions)
			try
			{
				internal_state->py_instance.attr("tick")(tick_info.delta_time, py_in, py_out);
			}
			catch (const py::error_already_set& e)
			{
				ROBOTICK_WARNING("Python tick() failed: %s", e.what());
			}

			for (auto item : py_out)
			{
				std::string key = py::str(item.first);
				auto val = item.second;

				const StructDescriptor& struct_desc = outputs.blackboard.get_struct_descriptor();
				const FieldDescriptor* found_field = struct_desc.find_field(key.c_str());

				if (!found_field)
					continue;

				if (found_field->type_id == GET_TYPE_ID(int))
					outputs.blackboard.set<int>(key, val.cast<int>());
				else if (found_field->type_id == GET_TYPE_ID(double))
					outputs.blackboard.set<double>(key, val.cast<double>());
				else if (found_field->type_id == GET_TYPE_ID(FixedString64))
					outputs.blackboard.set<FixedString64>(key, FixedString64(val.cast<std::string>().c_str()));
				else if (found_field->type_id == GET_TYPE_ID(FixedString128))
					outputs.blackboard.set<FixedString128>(key, FixedString128(val.cast<std::string>().c_str()));
			}
		}
	};

	ROBOTICK_REGISTER_WORKLOAD(PythonWorkload, PythonConfig, PythonInputs, PythonOutputs)

} // namespace robotick
