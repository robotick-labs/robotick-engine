// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/common/Hash.h"
#include "robotick/framework/common/List.h"

#include <string.h> // for strcmp

namespace robotick
{
	template <typename Key, typename Value> struct MapEntry
	{
		Key key;
		Value value;
	};

	template <typename Key, typename Value> struct MapBucket
	{
		List<MapEntry<Key, Value>> entries;
	};

	// --- Default Hash + Equals Traits ---
	template <typename T> struct DefaultHash
	{
		static size_t hash(const T& value) { return static_cast<size_t>(value); }
	};

	template <typename T> struct DefaultEqual
	{
		static bool equal(const T& a, const T& b) { return a == b; }
	};

	// --- Specialization for const char* ---
	template <> struct DefaultHash<const char*>
	{
		static size_t hash(const char* str) { return hash_string(str); }
	};

	template <> struct DefaultEqual<const char*>
	{
		static bool equal(const char* a, const char* b) { return strcmp(a, b) == 0; }
	};

	template <> struct DefaultHash<char*>
	{
		static size_t hash(const char* str) { return hash_string(str); }
	};

	template <> struct DefaultEqual<char*>
	{
		static bool equal(const char* a, const char* b) { return strcmp(a, b) == 0; }
	};

	template <typename Key, typename Value, size_t NumBuckets = 32> class Map
	{
	  public:
		using Entry = MapEntry<Key, Value>;
		using Bucket = MapBucket<Key, Value>;

		Map() { buckets.fill(); }

		void insert(const Key& key, const Value& value)
		{
			size_t index = DefaultHash<Key>::hash(key) % NumBuckets;
			Bucket& bucket = buckets[index];

			for (Entry& entry : bucket.entries)
			{
				if (DefaultEqual<Key>::equal(entry.key, key))
				{
					entry.value = value;
					return;
				}
			}
			bucket.entries.push_back({key, value});
		}

		Value* find(const Key& key)
		{
			size_t index = DefaultHash<Key>::hash(key) % NumBuckets;
			Bucket& bucket = buckets[index];

			for (Entry& entry : bucket.entries)
			{
				if (DefaultEqual<Key>::equal(entry.key, key))
					return &entry.value;
			}
			return nullptr;
		}

		const Value* find(const Key& key) const { return const_cast<Map*>(this)->find(key); }

	  private:
		FixedVector<Bucket, NumBuckets> buckets;
	};
} // namespace robotick
