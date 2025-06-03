// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"

#include "robotick/api_base.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"
#include <cstring>
#include <stdexcept>

namespace robotick
{
	uint8_t* BlackboardFieldInfo::get_data_ptr(Blackboard& blackboard) const
	{
		void* blackboard_ptr = static_cast<void*>(&blackboard);
		return static_cast<uint8_t*>(blackboard_ptr) + blackboard.get_datablock_offset() + this->offset_from_datablock;
	}

	const uint8_t* BlackboardFieldInfo::get_data_ptr(const Blackboard& blackboard) const
	{
		const void* blackboard_ptr = static_cast<const void*>(&blackboard);
		return static_cast<const uint8_t*>(blackboard_ptr) + blackboard.get_datablock_offset() + this->offset_from_datablock;
	}

	bool BlackboardInfo::has_field(const std::string& key) const
	{
		return schema_index_by_name.count(key) > 0;
	}

	const BlackboardFieldInfo* BlackboardInfo::find_field(const std::string& key) const
	{
		auto it = schema_index_by_name.find(key);
		if (it == schema_index_by_name.end())
			return nullptr;
		return &schema[it->second];
	}

	void BlackboardInfo::verify_type_by_size(const std::string& key, size_t expected_size) const
	{
		const auto* field = find_field(key);

		if (!field)
			ROBOTICK_FATAL_EXIT("Blackboard::verify_type_by_size failed, missing key: %s", key.c_str());

		if (field->size != expected_size)
			ROBOTICK_FATAL_EXIT("Blackboard::verify_type_by_size failed, type mismatch for key: %s", key.c_str());
	}

	void* BlackboardInfo::get_field_ptr(Blackboard* bb, const std::string& key) const
	{
		auto it = schema_index_by_name.find(key);
		if (it == schema_index_by_name.end())
			ROBOTICK_FATAL_EXIT("BlackboardInfo::get_field_ptr failed for key: %s", key.c_str());
		if (datablock_offset_from_blackboard == OFFSET_UNBOUND)
			ROBOTICK_FATAL_EXIT("Blackboard is not bound");

		const auto& field = schema[it->second];
		uint8_t* base = static_cast<uint8_t*>((void*)bb);
		return base + datablock_offset_from_blackboard + field.offset_from_datablock;
	}

	const void* BlackboardInfo::get_field_ptr(const Blackboard* bb, const std::string& key) const
	{
		return const_cast<BlackboardInfo*>(this)->get_field_ptr(const_cast<Blackboard*>(bb), key);
	}

	std::pair<size_t, size_t> BlackboardInfo::type_size_and_align(TypeId type)
	{
		if (type == GET_TYPE_ID(int))
			return {sizeof(int), alignof(int)};
		if (type == GET_TYPE_ID(double))
			return {sizeof(double), alignof(double)};
		if (type == GET_TYPE_ID(FixedString64))
			return {sizeof(FixedString64), alignof(FixedString64)};
		if (type == GET_TYPE_ID(FixedString128))
			return {sizeof(FixedString128), alignof(FixedString128)};

		ROBOTICK_FATAL_EXIT("Unsupported type in BlackboardInfo::type_size_and_align");
		return {0, 1};
	}

	Blackboard::Blackboard(const std::vector<BlackboardFieldInfo>& source_schema)
	{
		info = std::make_shared<BlackboardInfo>();
		info->schema.reserve(source_schema.size());

		size_t offset = 0;
		for (size_t i = 0; i < source_schema.size(); ++i)
		{
			BlackboardFieldInfo field = source_schema[i];

			auto [size, align] = BlackboardInfo::type_size_and_align(field.type);
			if (align == 0)
			{
				ROBOTICK_FATAL_EXIT("Invalid align (0) while building blackboard schema");
			}

			offset = (offset + align - 1) & ~(align - 1);
			field.offset_from_datablock = offset;
			field.size = size;

			info->schema_index_by_name[field.name.c_str()] = i;
			info->schema.push_back(field);
			offset += size;
		}
		info->total_datablock_size = offset;
	}

	void Blackboard::bind(size_t datablock_offset)
	{
		if (info)
			info->datablock_offset_from_blackboard = datablock_offset;
		else
			ROBOTICK_FATAL_EXIT("Blackboard::bind called on uninitialized Blackboard");
	}

	size_t Blackboard::get_datablock_offset() const
	{
		if (!info)
			ROBOTICK_FATAL_EXIT("Blackboard::get_datablock_offset called on uninitialized Blackboard");

		if (info->datablock_offset_from_blackboard == OFFSET_UNBOUND)
			ROBOTICK_FATAL_EXIT("Blackboard::get_datablock_offset called on Blackboard before datablock_offset has been set");

		return info->datablock_offset_from_blackboard;
	}

	const std::vector<BlackboardFieldInfo>& Blackboard::get_schema() const
	{
		if (!info)
			ROBOTICK_FATAL_EXIT("Blackboard::get_schema called on uninitialized Blackboard");

		return info->schema;
	}

	const BlackboardInfo* Blackboard::get_info() const
	{
		if (!info)
			ROBOTICK_FATAL_EXIT("Blackboard::get_info called on uninitialized Blackboard");

		return info.get();
	}

	const BlackboardFieldInfo* Blackboard::get_field_info(const std::string& key) const
	{
		if (!info)
			ROBOTICK_FATAL_EXIT("Blackboard::get_field_info called on uninitialized Blackboard");

		return info->find_field(key);
	}

	template <typename T> void Blackboard::set(const std::string& key, const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "Blackboard::set only supports trivially-copyable types");
		info->verify_type_by_size(key, sizeof(T));
		void* ptr = info->get_field_ptr(this, key);
		std::memcpy(ptr, &value, sizeof(T));
	}

	template <typename T> T Blackboard::get(const std::string& key) const
	{
		static_assert(std::is_trivially_copyable_v<T>, "Blackboard::get only supports trivially-copyable types");
		info->verify_type_by_size(key, sizeof(T));
		const void* ptr = info->get_field_ptr(this, key);
		T out;
		std::memcpy(&out, ptr, sizeof(T));
		return out;
	}

	// Explicit template instantiations
	template void Blackboard::set<int>(const std::string&, const int&);
	template void Blackboard::set<double>(const std::string&, const double&);
	template void Blackboard::set<FixedString64>(const std::string&, const FixedString64&);
	template void Blackboard::set<FixedString128>(const std::string&, const FixedString128&);

	template int Blackboard::get<int>(const std::string&) const;
	template double Blackboard::get<double>(const std::string&) const;
	template FixedString64 Blackboard::get<FixedString64>(const std::string&) const;
	template FixedString128 Blackboard::get<FixedString128>(const std::string&) const;

} // namespace robotick