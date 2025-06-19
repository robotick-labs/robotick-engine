#pragma once

#include <stddef.h> // for size_t
#include <string.h> // for memcpy, strncmp

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

		FixedString& operator=(const char* str)
		{
			const size_t len = min_val(fixed_strlen(str), N - 1);
			memcpy(data, str, len);
			data[len] = '\0';
			return *this;
		}

		bool operator<(const FixedString<N>& other) const noexcept { return strncmp(data, other.data, N) < 0; }

		const char* c_str() const { return data; }

		operator const char*() const { return data; }

		bool operator==(const char* other) const noexcept { return strncmp(data, other, N) == 0; }

		bool operator==(const FixedString<N>& other) const { return strncmp(data, other.data, N) == 0; }

		bool operator!=(const FixedString<N>& other) const { return !(*this == other); }

		bool empty() const { return data[0] == '\0'; }
	};

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

#include <functional> // std::hash
#include <string>
#include <string_view>

// Hash function (must be defined outside any namespace)
namespace std
{
	template <size_t N> struct hash<robotick::FixedString<N>>
	{
		size_t operator()(const robotick::FixedString<N>& s) const noexcept
		{
			// Use the part before the null terminator
			return std::hash<std::string_view>{}(std::string_view(s.c_str(), robotick::fixed_strlen(s.c_str())));
		}
	};
} // namespace std
