// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/FixedString.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace robotick
{

	// === Field types allowed in blackboards ===
	enum class BlackboardFieldType
	{
		Int,
		Double,
		FixedString64,
		FixedString128,
	};

	// A single named field with known type
	struct BlackboardField
	{
		FixedString64 name;
		BlackboardFieldType type;
	};

	// === A flat memory block with named fields and typed access ===
	class Blackboard
	{
	  public:
		Blackboard() = default;
		explicit Blackboard(const std::vector<BlackboardField>& schema);

		void bind(uint8_t* external_memory); // Caller owns the buffer

		size_t required_size() const;

		const std::vector<BlackboardField>& get_schema() const;

		template <typename T> void set(const std::string& key, const T& value);

		template <typename T> T get(const std::string& key) const;

		bool has(const std::string& key) const;

	  private:
		std::vector<BlackboardField> schema;
		std::unordered_map<std::string, size_t> offsets;
		size_t total_size = 0;
		uint8_t* base_ptr = nullptr;

		void* get_ptr(const std::string& key);
		const void* get_ptr(const std::string& key) const;

		size_t type_size(BlackboardFieldType type) const;
		size_t type_align(BlackboardFieldType type) const;
	};

} // namespace robotick