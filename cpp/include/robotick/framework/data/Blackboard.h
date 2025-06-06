// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/utils/Constants.h"
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace robotick
{
	class Blackboard;
	class Engine;
	struct BlackboardTestUtils;
	struct DataConnectionUtils;
	struct DataConnectionsFactory;
	struct WorkloadFieldsIterator;

	struct BlackboardFieldInfo
	{
		FixedString64 name;
		std::type_index type;
		size_t offset_from_datablock = 0;
		size_t size = 0;

		uint8_t* get_data_ptr(Blackboard& blackboard) const;
		const uint8_t* get_data_ptr(const Blackboard& blackboard) const;

		BlackboardFieldInfo(const FixedString64& name, std::type_index type) : name(name), type(type) {}
	};

	struct BlackboardInfo
	{
		std::vector<BlackboardFieldInfo> schema;
		std::unordered_map<std::string, size_t> schema_index_by_name;

		size_t total_datablock_size = 0;
		size_t datablock_offset_from_blackboard = OFFSET_UNBOUND;

		bool has_field(const std::string& key) const;
		const BlackboardFieldInfo* find_field(const std::string& key) const;
		void verify_type(const std::string& key, std::type_index expected) const;
		void* get_field_ptr(Blackboard* bb, const std::string& key) const;
		const void* get_field_ptr(const Blackboard* bb, const std::string& key) const;

		static std::pair<size_t, size_t> type_size_and_align(std::type_index type);
	};

	class Blackboard
	{
		friend class Engine;
		friend struct BlackboardFieldInfo;
		friend struct BlackboardInfo;
		friend struct BlackboardTestUtils;
		friend struct DataConnectionUtils;
		friend struct DataConnectionsFactory;
		friend struct PythonWorkload;
		friend struct WorkloadFieldsIterator;

	  public:
		Blackboard() = default;
		explicit Blackboard(const std::vector<BlackboardFieldInfo>& schema);

		Blackboard(const Blackboard&) = default;
		Blackboard& operator=(const Blackboard&) = default;

		bool has(const std::string& key) const { return info && info->has_field(key); }
		template <typename T> void set(const std::string& key, const T& value);
		template <typename T> T get(const std::string& key) const;

	  protected:
		void bind(size_t datablock_offset);
		size_t get_datablock_offset() const;

		const std::vector<BlackboardFieldInfo>& get_schema() const;
		const BlackboardInfo* get_info() const;
		const BlackboardFieldInfo* get_field_info(const std::string& key) const;

	  private:
		std::shared_ptr<BlackboardInfo> info = nullptr;
	};
} // namespace robotick