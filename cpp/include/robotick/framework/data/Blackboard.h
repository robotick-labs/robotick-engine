// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/FixedString.h"
#include <cstdint>
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

		BlackboardField(const FixedString64& name, std::type_index type) : name(name), type(type) {}
	};

	class Blackboard
	{
	  public:
		Blackboard() = default;
		explicit Blackboard(const std::vector<BlackboardField>& schema);

		void bind(size_t buffer_offset_in); // sets the offset of the blackboard's fields-data relative to ANY BlackboardsBuffer
		size_t required_size() const;

		uint8_t* get_base_ptr() const;

		const std::vector<BlackboardField>& get_schema() const;
		bool has(const std::string& key) const;

		template <typename T> void set(const std::string& key, const T& value);

		template <typename T> T get(const std::string& key) const;

	  private:
		std::vector<BlackboardField> schema;
		std::unordered_map<std::string, size_t> offsets;
		std::unordered_map<std::string, std::type_index> types;
		size_t total_size = 0;
		size_t buffer_offset = 0;

		void* get_ptr(const std::string& key);
		const void* get_ptr(const std::string& key) const;

		size_t type_size(std::type_index type) const;
		size_t type_align(std::type_index type) const;

		void verify_type(const std::string& key, std::type_index expected) const;
	};

} // namespace robotick
