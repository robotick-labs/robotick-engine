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

	Blackboard::Blackboard(const std::vector<BlackboardField>& source_schema)
	{
		schema.reserve(source_schema.size());
		size_t offset = 0;

		size_t index = 0;

		for (const auto& field : source_schema)
		{
			BlackboardField& field_copy = schema.emplace_back(field); // invokes copy constructor

			const std::type_index& type = field_copy.type;
			const size_t align = type_align(type);
			const size_t size = type_size(type);

			// Align offset
			offset = (offset + align - 1) & ~(align - 1);
			field_copy.offset = offset;
			field_copy.size = size;

			offset += size;

			// Store into schema and map
			schema_index_by_name[field_copy.name.c_str()] = index;
			index++;
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

	const std::vector<BlackboardField>& Blackboard::get_schema() const
	{
		return schema;
	}

	const BlackboardField* Blackboard::get_schema_field(const std::string& key) const
	{
		auto it = schema_index_by_name.find(key);
		if (it == schema_index_by_name.end())
		{
			return nullptr;
		}

		return &schema[schema_index_by_name.at(key)];
	}

	bool Blackboard::has(const std::string& key) const
	{
		return schema_index_by_name.count(key) > 0;
	}

	void* Blackboard::get_ptr(const std::string& key)
	{
		uint8_t* base_ptr = get_base_ptr();

		auto it = schema_index_by_name.find(key);
		if (it == schema_index_by_name.end() || base_ptr == nullptr)
			throw std::runtime_error("Blackboard::get_ptr failed for key: " + key);

		const BlackboardField& field = schema[schema_index_by_name.at(key)];
		return base_ptr + field.offset;
	}

	const void* Blackboard::get_ptr(const std::string& key) const
	{
		uint8_t* base_ptr = get_base_ptr();

		auto it = schema_index_by_name.find(key);
		if (it == schema_index_by_name.end() || base_ptr == nullptr)
			throw std::runtime_error("Blackboard::get_ptr failed for key: " + key);

		const BlackboardField& field = schema[schema_index_by_name.at(key)];
		return base_ptr + field.offset;
	}

	void Blackboard::verify_type(const std::string& key, std::type_index expected) const
	{
		auto it = schema_index_by_name.find(key);
		if (it == schema_index_by_name.end())
			throw std::runtime_error("Blackboard::verify_type failed, missing key: " + key);

		const BlackboardField& field = schema[it->second];

		if (field.type != expected)
			throw std::runtime_error("Blackboard::verify_type failed, type mismatch for key: " + key);
	}

	size_t Blackboard::type_size(std::type_index type) const
	{
		if (type == typeid(int))
			return sizeof(int);
		if (type == typeid(double))
			return sizeof(double);
		if (type == typeid(FixedString64))
			return sizeof(FixedString64);
		if (type == typeid(FixedString128))
			return sizeof(FixedString128);
		throw std::runtime_error("Unsupported type in Blackboard::type_size");
	}

	size_t Blackboard::type_align(std::type_index type) const
	{
		if (type == typeid(int))
			return alignof(int);
		if (type == typeid(double))
			return alignof(double);
		if (type == typeid(FixedString64))
			return alignof(FixedString64);
		if (type == typeid(FixedString128))
			return alignof(FixedString128);
		throw std::runtime_error("Unsupported type in Blackboard::type_align");
	}

	template <typename T> void Blackboard::set(const std::string& key, const T& value)
	{
		verify_type(key, typeid(T));
		void* field_data_ptr = get_ptr(key);
		std::memcpy(field_data_ptr, &value, sizeof(T));
	}

	template <typename T> T Blackboard::get(const std::string& key) const
	{
		verify_type(key, typeid(T));
		T out;
		const void* field_data_ptr = get_ptr(key);
		std::memcpy(&out, field_data_ptr, sizeof(T));
		return out;
	}

	// Explicit instantiations
	template void Blackboard::set<int>(const std::string&, const int&);
	template void Blackboard::set<double>(const std::string&, const double&);
	template void Blackboard::set<FixedString64>(const std::string&, const FixedString64&);
	template void Blackboard::set<FixedString128>(const std::string&, const FixedString128&);

	template int Blackboard::get<int>(const std::string&) const;
	template double Blackboard::get<double>(const std::string&) const;
	template FixedString64 Blackboard::get<FixedString64>(const std::string&) const;
	template FixedString128 Blackboard::get<FixedString128>(const std::string&) const;

} // namespace robotick
