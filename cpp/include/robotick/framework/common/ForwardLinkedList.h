// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>

namespace robotick
{
	/**
	 * @brief A lightweight singly-linked list with intrusive node linkage.
	 *
	 * Assumes that T has a public member: T* next_entry = nullptr;
	 * This avoids all allocations and inheritance. T is responsible for null-initializing its 'next_entry' attribute.
	 */
	template <typename T> class ForwardLinkedList
	{
	  public:
		ForwardLinkedList() = default;

		void add(T& item)
		{
			item.next_entry = head;
			head = &item;
			num_entries++;
		}

		struct Iterator
		{
			explicit Iterator(T* ptr) : current(ptr) {}

			T& operator*() const { return *current; }
			T* operator->() const { return current; }

			Iterator& operator++()
			{
				current = current->next_entry;
				return *this;
			}

			bool operator!=(const Iterator& other) const { return current != other.current; }

		  private:
			T* current;
		};

		Iterator begin() { return Iterator{head}; }
		Iterator end() { return Iterator{nullptr}; }

		T* front() { return head; }
		bool empty() const { return head == nullptr; }

		size_t size() const { return num_entries; };

	  private:
		T* head = nullptr;
		size_t num_entries = 0;
	};
} // namespace robotick
