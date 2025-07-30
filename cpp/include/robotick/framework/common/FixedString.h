// Copyright Robotick Labs
#pragma once

#include "robotick/framework/common/Hash.h"

#include <cstdio>
#include <stddef.h>
#include <string.h>
#include <utility>

namespace robotick
{
	// constexpr min function
	template <typename T> constexpr T min_val(T a, T b)
	{
		return (a < b) ? a : b;
	}

	// Lightweight replacement for strlen
	constexpr size_t fixed_strlen(const char* str)
	{
		size_t len = 0;
		while (str[len] != '\0')
			++len;
		return len;
	}

	template <size_t N> struct FixedString
	{
		static_assert(N > 1, "FixedString must be at least 2 characters long (incl null)");

		char data[N] = {};

		FixedString() = default;

		FixedString(const char* str)
		{
			const size_t len = min_val(fixed_strlen(str), N - 1);
			memcpy(data, str, len);
			data[len] = '\0';
		}

		FixedString(const char* str, const size_t max_copy_length)
		{
			if (!str)
			{
				data[0] = '\0';
				return;
			}

			const size_t len = min_val(fixed_strlen(str), min_val(max_copy_length, N - 1));
			memcpy(data, str, len);
			data[len] = '\0';
		}

		FixedString& operator=(const char* str)
		{
			const size_t len = min_val(fixed_strlen(str), N - 1);
			memcpy(data, str, len);
			data[len] = '\0';
			return *this;
		}

		bool operator<(const FixedString<N>& other) const noexcept { return strcmp(data, other.data) < 0; }

		char* str() { return data; }

		const char* c_str() const { return data; }

		operator const char*() const { return data; }

		bool equals(const char* other) const noexcept { return strcmp(data, other) == 0; }

		bool operator==(const char* other) const noexcept { return strcmp(data, other) == 0; }

		bool operator==(const FixedString<N>& other) const { return strcmp(data, other.data) == 0; }

		bool operator!=(const FixedString<N>& other) const { return !(*this == other); }

		bool empty() const { return data[0] == '\0'; }

		bool contains(const char query_char) const
		{
			const size_t str_length = length();
			for (size_t i = 0; i < str_length; ++i)
			{
				if (data[i] == query_char)
					return true;
			}
			return false;
		}

		size_t length() const { return fixed_strlen(data); };

		constexpr size_t capacity() const { return N; }

		template <typename... Args> void format(const char* fmt, Args&&... args)
		{
			const int written = std::snprintf(data, N, fmt, std::forward<Args>(args)...);
			if (written < 0)
			{
				data[0] = '\0';
			}
		}
	};

	template <size_t N> inline size_t hash(const FixedString<N>& s)
	{
		return hash_string(s.data);
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
