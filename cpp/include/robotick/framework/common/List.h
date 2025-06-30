// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

namespace robotick
{
	/**
	 * @brief A minimal singly-linked list that owns each element inline.
	 *
	 * `List<T>` is a forward-only, dynamically growable list structure. Each node owns its `T` value directly (no indirection),
	 * and all memory allocations are explicit — one per node. Unlike `HeapVector`, this avoids all reallocation or shifting
	 * and is ideal for runtime-sized collections with append-only behavior.
	 *
	 * Typical usage:
	 *   List<Foo> list;
	 *   list.push_back(Foo{...});
	 *   for (Foo& item : list) { ... }
	 *
	 * This container is designed to be friendly to Robotick’s memory model. While it uses heap allocation, each node is
	 * individually controlled and visible, making it compatible with the custom `MemoryManager` if needed.
	 *
	 * @tparam T The value type to store. Must be movable or copyable.
	 */
	template <typename T> class List
	{
	  public:
		struct Node
		{
			T value;
			Node* next = nullptr;
		};

		class Iterator
		{
		  public:
			Iterator(Node* ptr) : current(ptr) {}
			T& operator*() const { return current->value; }
			T* operator->() const { return &current->value; }
			Iterator& operator++()
			{
				current = current->next;
				return *this;
			}
			bool operator!=(const Iterator& other) const { return current != other.current; }

		  private:
			Node* current;
		};

		List() = default;

		~List() { clear(); }

		T& push_back()
		{
			Node* node = new Node;
			if (tail)
			{
				tail->next = node;
			}
			else
			{
				head = node;
			}
			tail = node;
			list_size++;

			return node->value;
		}

		T& push_back(const T& value)
		{
			Node* node = new Node{value, nullptr};
			if (tail)
			{
				tail->next = node;
			}
			else
			{
				head = node;
			}
			tail = node;
			list_size++;

			return node->value;
		}

		T& push_back(T&& value)
		{
			Node* node = new Node{static_cast<T&&>(value), nullptr};
			if (tail)
			{
				tail->next = node;
			}
			else
			{
				head = node;
			}
			tail = node;
			list_size++;

			return node->value;
		}

		Iterator begin() { return Iterator{head}; }
		Iterator end() { return Iterator{nullptr}; }

		const Iterator begin() const { return Iterator{head}; }
		const Iterator end() const { return Iterator{nullptr}; }

		void clear()
		{
			Node* node = head;
			while (node)
			{
				Node* next = node->next;
				delete node;
				node = next;
			}
			head = nullptr;
			tail = nullptr;
			list_size = 0;
		}

		bool empty() const { return head == nullptr; }

		size_t size() const { return list_size; }

	  private:
		Node* head = nullptr;
		Node* tail = nullptr;
		size_t list_size = 0;
	};

} // namespace robotick
