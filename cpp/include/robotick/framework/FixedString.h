// Copyright 2025 Robotick Labs CIC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#pragma once

#include <cstring>
#include <iostream>
#include <string>

namespace robotick
{

	template <size_t N> struct FixedString
	{
		static_assert(N > 1, "FixedString must be at least 2 characters long (incl null)");

		char data[N] = {};

		FixedString() = default;

		FixedString(const char *str)
		{
			strncpy(data, str, N - 1);
			data[N - 1] = '\0';
		}

		FixedString &operator=(const char *str)
		{
			strncpy(data, str, N - 1);
			data[N - 1] = '\0';
			return *this;
		}

		const char *c_str() const
		{
			return data;
		}

		operator const char *() const
		{
			return data;
		}

		bool operator==(const FixedString<N> &other) const
		{
			return std::strncmp(data, other.data, N) == 0;
		}

		bool operator!=(const FixedString<N> &other) const
		{
			return !(*this == other);
		}

		bool empty() const
		{
			return data[0] == '\0';
		}

		std::string to_string() const
		{
			return std::string(data);
		}
	};

	// Stream output (e.g., for logging)
	template <size_t N> inline std::ostream &operator<<(std::ostream &os, const FixedString<N> &str)
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
