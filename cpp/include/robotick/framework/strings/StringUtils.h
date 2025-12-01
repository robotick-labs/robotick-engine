// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

namespace robotick
{
	inline size_t string_length(const char* str)
	{
		if (!str)
			return 0;
		size_t len = 0;
		while (*str++)
			++len;
		return len;
	}

	inline bool string_equals(const char* a, const char* b)
	{
		if (a == b)
			return true;
		if (!a || !b)
			return false;
		while (*a && (*a == *b))
		{
			++a;
			++b;
		}
		return *a == *b;
	}

	inline bool string_contains(const char* haystack, const char* needle)
	{
		if (haystack == needle)
			return true;
		if (!haystack || !needle)
			return false;
		if (*needle == '\0')
			return true;

		const char first = *needle;
		const char* needle_rest = needle + 1;
		for (const char* h = haystack; *h; ++h)
		{
			if (*h != first)
				continue;

			const char* h_it = h + 1;
			const char* n_it = needle_rest;
			while (*n_it && *h_it == *n_it)
			{
				++h_it;
				++n_it;
			}
			if (*n_it == '\0')
				return true;
		}
		return false;
	}

} // namespace robotick
