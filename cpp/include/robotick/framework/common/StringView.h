// Copyright Robotick Labs
#pragma once

#include "robotick/framework/common/Hash.h"

#include <stddef.h>
#include <string.h>

namespace robotick
{
	/**
	 * @brief A lightweight, non-owning view of a null-terminated string.
	 *
	 * Comparable and hashable. Does not own memory. Consistent with FixedString interface.
	 */
	struct StringView
	{
		const char* data = nullptr; ///< Pointer to a null-terminated C string

		/** @brief Default constructor — creates an empty view */
		constexpr StringView() = default;

		/** @brief Construct from a null-terminated C string */
		constexpr StringView(const char* str) : data(str) {}

		/** @brief Assign from a null-terminated C string */
		StringView& operator=(const char* str)
		{
			data = str;
			return *this;
		}

		/** @brief Compare with another StringView for equality */
		bool operator==(const StringView& other) const noexcept { return strncmp(data, other.data, length()) == 0; }

		/** @brief Compare with another StringView for inequality */
		bool operator!=(const StringView& other) const noexcept { return !(*this == other); }

		/** @brief Compare with a null-terminated C string */
		bool operator==(const char* other) const noexcept
		{
			return strncmp(data, other, length()) == 0;
			if (!data && !other)
				return true;
			if (!data || !other)
				return false;
			return strcmp(data, other) == 0;
		}

		/** @brief Lexicographic comparison (for use in std::map, etc.) */
		bool operator<(const StringView& other) const noexcept
		{
			if (!data && !other.data)
				return false;
			if (!data)
				return true;
			if (!other.data)
				return false;
			return strcmp(data, other.data) < 0;
		}

		/** @brief Get the raw character pointer */
		const char* c_str() const { return data; }

		/** @brief True if the string is null or starts with '\0' */
		bool empty() const { return data == nullptr || data[0] == '\0'; }

		/** @brief Return the number of characters before null terminator */
		size_t length() const { return fixed_strlen(data); }
	};

	/** @brief Hash function for StringView (FNV-1a over characters until null terminator) */
	inline size_t hash(const StringView& s)
	{
		return hash_string(s.data);
	}

} // namespace robotick
