// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"
#include <cstring>
#include <stdexcept>

namespace robotick
{

	BlackboardInstance::BlackboardInstance(const std::vector<BlackboardField>& schema) : schema(schema)
	{
		size_t offset = 0;
		for (const auto& field : schema)
		{
			size_t align = type_align(field.type);
			size_t size = type_size(field.type);

			// Align offset
			offset = (offset + align - 1) & ~(align - 1);

			offsets[field.name.c_str()] = offset;
			offset += size;
		}
		total_size = offset;
	}

	void BlackboardInstance::bind(uint8_t* external_memory)
	{
		base_ptr = external_memory;
	}

	size_t BlackboardInstance::required_size() const
	{
		return total_size;
	}

	const std::vector<BlackboardField>& BlackboardInstance::get_schema() const
	{
		return schema;
	}

	bool BlackboardInstance::has(const std::string& key) const
	{
		return offsets.count(key) > 0;
	}

	size_t BlackboardInstance::type_size(BlackboardFieldType type) const
	{
		switch (type)
		{
		case BlackboardFieldType::Int:
			return sizeof(int);
		case BlackboardFieldType::Double:
			return sizeof(double);
		case BlackboardFieldType::FixedString64:
			return sizeof(FixedString64);
		case BlackboardFieldType::FixedString128:
			return sizeof(FixedString128);
		}
		throw std::runtime_error("Unknown BlackboardFieldType");
	}

	size_t BlackboardInstance::type_align(BlackboardFieldType type) const
	{
		switch (type)
		{
		case BlackboardFieldType::Int:
			return alignof(int);
		case BlackboardFieldType::Double:
			return alignof(double);
		case BlackboardFieldType::FixedString64:
			return alignof(FixedString64);
		case BlackboardFieldType::FixedString128:
			return alignof(FixedString128);
		}
		throw std::runtime_error("Unknown BlackboardFieldType");
	}

	void* BlackboardInstance::get_ptr(const std::string& key)
	{
		auto it = offsets.find(key);
		if (it == offsets.end() || base_ptr == nullptr)
		{
			throw std::runtime_error("Blackboard get_ptr() failed: " + key);
		}
		return base_ptr + it->second;
	}

	const void* BlackboardInstance::get_ptr(const std::string& key) const
	{
		auto it = offsets.find(key);
		if (it == offsets.end() || base_ptr == nullptr)
		{
			throw std::runtime_error("Blackboard get_ptr() failed: " + key);
		}
		return base_ptr + it->second;
	}

	template <typename T> void BlackboardInstance::set(const std::string& key, const T& value)
	{
		std::memcpy(get_ptr(key), &value, sizeof(T));
	}

	template <typename T> T BlackboardInstance::get(const std::string& key) const
	{
		T out;
		std::memcpy(&out, get_ptr(key), sizeof(T));
		return out;
	}

	// Explicit instantiations
	template void BlackboardInstance::set<int>(const std::string&, const int&);
	template void BlackboardInstance::set<double>(const std::string&, const double&);
	template void BlackboardInstance::set<FixedString64>(const std::string&, const FixedString64&);
	template void BlackboardInstance::set<FixedString128>(const std::string&, const FixedString128&);

	template int BlackboardInstance::get<int>(const std::string&) const;
	template double BlackboardInstance::get<double>(const std::string&) const;
	template FixedString64 BlackboardInstance::get<FixedString64>(const std::string&) const;
	template FixedString128 BlackboardInstance::get<FixedString128>(const std::string&) const;

} // namespace robotick
