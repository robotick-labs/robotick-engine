// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cstring>
#include <functional> // std::hash
#include <iostream>
#include <string>
#include <string_view>

namespace robotick
{

	template <size_t N> struct FixedString
	{
		static_assert(N > 1, "FixedString must be at least 2 characters long (incl null)");

		char data[N] = {};

		FixedString() = default;

		FixedString(const char* str)
		{
			const size_t len = std::min(std::strlen(str), N - 1);
			std::memcpy(data, str, len);
			data[len] = '\0';
		}

		FixedString(const std::string& str)
		{
			const size_t len = std::min(str.size(), N - 1);
			std::memcpy(data, str.c_str(), len);
			data[len] = '\0';
		}

		FixedString& operator=(const char* str)
		{
			const size_t len = std::min(std::strlen(str), N - 1);
			std::memcpy(data, str, len);
			data[len] = '\0';
			return *this;
		}

		FixedString& operator=(const std::string& str)
		{
			const size_t len = std::min(str.size(), N - 1);
			std::memcpy(data, str.c_str(), len);
			data[len] = '\0';
			return *this;
		}

		bool operator<(const FixedString<N>& other) const noexcept { return std::strncmp(data, other.data, N) < 0; }

		const char* c_str() const { return data; }

		operator const char*() const { return data; }

		bool operator==(const char* other) const noexcept { return std::strncmp(data, other, N) == 0; }
		bool operator==(const FixedString<N>& other) const { return std::strncmp(data, other.data, N) == 0; }

		bool operator!=(const FixedString<N>& other) const { return !(*this == other); }

		bool empty() const { return data[0] == '\0'; }

		std::string to_string() const { return std::string(data); }
	};

	// Stream output (e.g., for logging)
	template <size_t N> inline std::ostream& operator<<(std::ostream& os, const FixedString<N>& str)
	{
		return os << str.c_str();
	}

	// Type aliases
	using FixedString8 = FixedString<8>;
	using FixedString16 = FixedString<16>;
	using FixedString32 = FixedString<32>;
	using FixedString64 = FixedString<64>;
	using FixedString128 = FixedString<128>;
	using FixedString256 = FixedString<256>;
	using FixedString512 = FixedString<512>;
	using FixedString1024 = FixedString<1024>;

} // namespace robotick

// Hash function (must be defined outside any namespace)
namespace std
{
	template <size_t N> struct hash<robotick::FixedString<N>>
	{
		size_t operator()(const robotick::FixedString<N>& s) const noexcept
		{
			// Use the part before the null terminator
			return std::hash<std::string_view>{}(std::string_view(s.c_str(), std::strlen(s.c_str())));
		}
	};
} // namespace std
