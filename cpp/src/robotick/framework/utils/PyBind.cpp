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

#include "robotick/framework/utils/PyBind.h"
#include "robotick/framework/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/FieldUtils.h"

#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace robotick
{

	template <size_t N> inline bool is_fixed_string_type(const std::type_index& t)
	{
		return t == typeid(FixedString<N>);
	}

	void marshal_struct_to_dict(void* struct_ptr, const StructRegistryEntry& info, py::dict& py_out)
	{
		for (const auto& field : info.fields)
		{
			void* field_ptr = static_cast<uint8_t*>(struct_ptr) + field.offset;

			if (dispatch_fixed_string(field.type,
					[&, field_ptr](auto T) -> bool
					{
						using FS = typename decltype(T)::type;
						py_out[field.name.c_str()] = static_cast<FS*>(field_ptr)->c_str();
						return true;
					}))
			{
				// handled by lambda
			}
			else if (field.type == typeid(double))
			{
				py_out[field.name.c_str()] = *static_cast<double*>(field_ptr);
			}
			else if (field.type == typeid(int))
			{
				py_out[field.name.c_str()] = *static_cast<int*>(field_ptr);
			}
			else
			{
				std::cerr << "[marshal_struct_to_dict] Unsupported field type for " << field.name << "\n";
			}
		}
	}

	void marshal_dict_to_struct(const py::dict& py_in, const StructRegistryEntry& info, void* struct_ptr)
	{
		for (const auto& field : info.fields)
		{
			if (!py_in.contains(field.name))
				continue;

			const py::handle& value = py_in[field.name.c_str()];
			void* field_ptr = static_cast<uint8_t*>(struct_ptr) + field.offset;

			try
			{
				if (dispatch_fixed_string(field.type,
						[&, field_ptr](auto T) -> bool
						{
							using FS = typename decltype(T)::type;
							std::string str = py::cast<std::string>(value);
							*static_cast<FS*>(field_ptr) = str.c_str();
							return true;
						}))
				{
					// handled by lambda
				}
				else if (field.type == typeid(double))
				{
					*static_cast<double*>(field_ptr) = py::cast<double>(value);
				}
				else if (field.type == typeid(int))
				{
					*static_cast<int*>(field_ptr) = py::cast<int>(value);
				}
				else
				{
					std::cerr << "[marshal_dict_to_struct] Unsupported field "
								 "type for '"
							  << field.name << "'\n";
				}
			}
			catch (const py::error_already_set& e)
			{
				std::cerr << "[marshal_dict_to_struct] Error converting field '" << field.name << "': " << e.what() << "\n";
			}
		}
	}

} // namespace robotick
