// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/containers/List.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/utility/Hash.h"

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

	template <size_t N> struct DefaultHash<FixedString<N>>
	{
		static size_t hash(const FixedString<N>& value) { return hash_string(value.c_str()); }
	};

	template <size_t N> struct DefaultEqual<FixedString<N>>
	{
		static bool equal(const FixedString<N>& a, const FixedString<N>& b) { return strcmp(a.c_str(), b.c_str()) == 0; }
	};

	template <typename Key, typename Value, size_t NumBuckets = 32> class Map
	{
	  public:
		using Entry = MapEntry<Key, Value>;
		using Bucket = MapBucket<Key, Value>;

		Map() { buckets.fill(); } // Initialize all buckets to enable indexing

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
			size_++;
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

		const Value* find(const Key& key) const
		{
			size_t index = DefaultHash<Key>::hash(key) % NumBuckets;
			const Bucket& bucket = buckets[index];

			for (const Entry& entry : bucket.entries)
			{
				if (DefaultEqual<Key>::equal(entry.key, key))
					return &entry.value;
			}
			return nullptr;
		}

		bool contains(const Key& key) const { return find(key) != nullptr; }

		size_t size() const { return size_; }

		void clear()
		{
			for (Bucket& bucket : buckets)
			{
				bucket.entries.clear();
			}
			size_ = 0;
		}

		template <typename Fn> void for_each(Fn&& fn)
		{
			for (Bucket& bucket : buckets)
			{
				for (Entry& entry : bucket.entries)
				{
					fn(entry.key, entry.value);
				}
			}
		}

		template <typename Fn> void for_each(Fn&& fn) const
		{
			for (const Bucket& bucket : buckets)
			{
				for (const Entry& entry : bucket.entries)
				{
					fn(entry.key, entry.value);
				}
			}
		}

	  private:
		FixedVector<Bucket, NumBuckets> buckets;
		size_t size_ = 0;
	};
} // namespace robotick
