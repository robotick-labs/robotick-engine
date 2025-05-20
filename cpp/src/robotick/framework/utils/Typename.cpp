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

#include "robotick/framework/utils/Typename.h"

#ifdef __GNUG__ // GCC/Clang only
#include <cxxabi.h>
#endif

#include <memory>
#include <string>
#include <typeindex>

std::string get_clean_typename(const std::type_index& t)
{
	std::string name = t.name();

#ifdef __GNUG__
	int status = -1;
	std::unique_ptr<char, void (*)(void*)> demangled(abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status), std::free);

	if (status == 0 && demangled)
		name = demangled.get();
#endif

	// Strip leading namespace, e.g. "robotick::" or "std::"
	const size_t ns_pos = name.rfind("::");
	if (ns_pos != std::string::npos)
		name = name.substr(ns_pos + 2);

	if (name.rfind("class ", 0) == 0)
		return name.substr(6);

	if (name.rfind("struct ", 0) == 0)
		return name.substr(7);

	return name;
}