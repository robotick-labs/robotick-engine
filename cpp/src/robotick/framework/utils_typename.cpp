
#include "robotick/framework/utils_typename.h"

#ifdef __GNUG__ // GCC/Clang only
#include <cxxabi.h>
#endif

#include <memory>
#include <string>
#include <typeindex>

std::string get_clean_typename(const std::type_index &t)
{
	std::string name = t.name();

#ifdef __GNUG__
	int status = -1;
	std::unique_ptr<char, void (*)(void *)> demangled(abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status),
													  std::free);

	if (status == 0 && demangled)
		name = demangled.get();
#endif

	if (name.rfind("class ", 0) == 0)
		return name.substr(6);
	if (name.rfind("struct ", 0) == 0)
		return name.substr(7);
	return name;
}