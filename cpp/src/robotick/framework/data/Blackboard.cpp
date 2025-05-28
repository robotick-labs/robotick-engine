// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/Buffer.h"
#include <cstring>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>

namespace robotick
{
	using namespace std;

	Blackboard::Blackboard(const vector<BlackboardField>& schema) : schema(schema)
	{
		size_t offset = 0;

		for (const auto& field : schema)
		{
			const type_index& type = field.type;
			const size_t align = type_align(type);
			const size_t size = type_size(type);

			// Align offset
			offset = (offset + align - 1) & ~(align - 1);

			const std::string key = field.name.c_str();
			offsets.emplace(key, offset);
			types.emplace(key, type);

			offset += size;
		}

		total_size = offset;
	}

	void Blackboard::bind(const size_t buffer_offset_in)
	{
		buffer_offset = buffer_offset_in;
	}

	uint8_t* Blackboard::get_base_ptr() const
	{
		if (buffer_offset == UNBOUND_OFFSET)
			throw std::runtime_error("Blackboard::get_base_ptr: blackboard is not bound to a buffer");

		BlackboardsBuffer& buffer = BlackboardsBuffer::get_source();
		return buffer.raw_ptr() + buffer_offset;
	}

	size_t Blackboard::required_size() const
	{
		return total_size;
	}

	const vector<BlackboardField>& Blackboard::get_schema() const
	{
		return schema;
	}

	bool Blackboard::has(const string& key) const
	{
		return offsets.count(key) > 0;
	}

	void* Blackboard::get_ptr(const string& key)
	{
		uint8_t* base_ptr = get_base_ptr();

		auto it = offsets.find(key);
		if (it == offsets.end() || base_ptr == nullptr)
			throw runtime_error("Blackboard::get_ptr failed for key: " + key);

		return base_ptr + it->second;
	}

	const void* Blackboard::get_ptr(const string& key) const
	{
		uint8_t* base_ptr = get_base_ptr();

		auto it = offsets.find(key);
		if (it == offsets.end() || base_ptr == nullptr)
			throw runtime_error("Blackboard::get_ptr failed for key: " + key);

		return base_ptr + it->second;
	}

	void Blackboard::verify_type(const string& key, type_index expected) const
	{
		auto it = types.find(key);
		if (it == types.end())
			throw runtime_error("Blackboard::verify_type failed, missing key: " + key);

		if (it->second != expected)
			throw runtime_error("Blackboard::verify_type failed, type mismatch for key: " + key);
	}

	size_t Blackboard::type_size(type_index type) const
	{
		if (type == typeid(int))
			return sizeof(int);
		if (type == typeid(double))
			return sizeof(double);
		if (type == typeid(FixedString64))
			return sizeof(FixedString64);
		if (type == typeid(FixedString128))
			return sizeof(FixedString128);
		throw runtime_error("Unsupported type in Blackboard::type_size");
	}

	size_t Blackboard::type_align(type_index type) const
	{
		if (type == typeid(int))
			return alignof(int);
		if (type == typeid(double))
			return alignof(double);
		if (type == typeid(FixedString64))
			return alignof(FixedString64);
		if (type == typeid(FixedString128))
			return alignof(FixedString128);
		throw runtime_error("Unsupported type in Blackboard::type_align");
	}

	template <typename T> void Blackboard::set(const string& key, const T& value)
	{
		verify_type(key, typeid(T));
		void* field_data_ptr = get_ptr(key);
		memcpy(field_data_ptr, &value, sizeof(T));
	}

	template <typename T> T Blackboard::get(const string& key) const
	{
		verify_type(key, typeid(T));
		T out;
		const void* field_data_ptr = get_ptr(key);
		memcpy(&out, field_data_ptr, sizeof(T));
		return out;
	}

	// Explicit instantiations
	template void Blackboard::set<int>(const string&, const int&);
	template void Blackboard::set<double>(const string&, const double&);
	template void Blackboard::set<FixedString64>(const string&, const FixedString64&);
	template void Blackboard::set<FixedString128>(const string&, const FixedString128&);

	template int Blackboard::get<int>(const string&) const;
	template double Blackboard::get<double>(const string&) const;
	template FixedString64 Blackboard::get<FixedString64>(const string&) const;
	template FixedString128 Blackboard::get<FixedString128>(const string&) const;

} // namespace robotick
