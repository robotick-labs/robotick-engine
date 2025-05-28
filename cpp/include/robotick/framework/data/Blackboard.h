// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include <cstdint>
#include <limits>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace robotick
{
	struct BlackboardField
	{
		FixedString64 name;
		std::type_index type;
		size_t offset = 0;
		size_t size = 0;

		BlackboardField(const FixedString64& name, std::type_index type) : name(name), type(type) {}
	};

	class Blackboard
	{
	  public:
		static constexpr size_t UNBOUND_OFFSET = std::numeric_limits<size_t>::max();

		Blackboard() = default;
		explicit Blackboard(const std::vector<BlackboardField>& schema);

		void bind(size_t buffer_offset_in); // sets the offset of the blackboard's fields-data relative to ANY BlackboardsBuffer
		size_t required_size() const;

		uint8_t* get_base_ptr() const;

		const std::vector<BlackboardField>& get_schema() const;
		const BlackboardField* get_schema_field(const std::string& key) const;

		bool has(const std::string& key) const;

		template <typename T> void set(const std::string& key, const T& value);

		template <typename T> T get(const std::string& key) const;

	  private:
		std::vector<BlackboardField> schema;
		std::unordered_map<std::string, size_t> schema_index_by_name;

		size_t total_size = 0;
		size_t buffer_offset = UNBOUND_OFFSET;

		void* get_ptr(const std::string& key);
		const void* get_ptr(const std::string& key) const;

		size_t type_size(std::type_index type) const;
		size_t type_align(std::type_index type) const;

		void verify_type(const std::string& key, std::type_index expected) const;
	};

} // namespace robotick
