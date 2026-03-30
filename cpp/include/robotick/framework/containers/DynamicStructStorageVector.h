// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace robotick
{
	template <typename T, uint32_t ElementTypeIdValue> class DynamicStructStorageVector
	{
	  public:
		using ValueType = T;
		using SelfType = DynamicStructStorageVector<T, ElementTypeIdValue>;

		void initialize_capacity(size_t new_capacity)
		{
			ROBOTICK_ASSERT_MSG(new_capacity > 0, "DynamicStructStorageVector::initialize_capacity() requires new_capacity > 0");
			ROBOTICK_ASSERT_MSG(new_capacity <= static_cast<size_t>(UINT32_MAX),
				"DynamicStructStorageVector::initialize_capacity() capacity %zu exceeds uint32_t range",
				new_capacity);

			initialize_descriptor();
			capacity_value = static_cast<uint32_t>(new_capacity);
			refresh_descriptor_runtime_fields();
		}

		void clear() { count = 0; }

		void set_size(size_t new_size)
		{
			ROBOTICK_ASSERT_MSG(new_size <= capacity(), "DynamicStructStorageVector::set_size() exceeds capacity");
			count = static_cast<uint32_t>(new_size);
		}

		size_t size() const { return count; }
		size_t capacity() const { return capacity_value; }
		bool empty() const { return count == 0; }
		bool full() const { return count == capacity_value; }

		void add(const T& value)
		{
			ROBOTICK_ASSERT_MSG(count < capacity_value, "DynamicStructStorageVector::add() - overflow");
			data()[count++] = value;
		}

		void set(const T* src, size_t len)
		{
			ROBOTICK_ASSERT(src != nullptr);
			ROBOTICK_ASSERT_MSG(len <= capacity(), "DynamicStructStorageVector::set() exceeds capacity");
			::memcpy(data(), src, len * sizeof(T));
			count = static_cast<uint32_t>(len);
		}

		void set_bytes(const void* src, size_t byte_count)
		{
			ROBOTICK_ASSERT(src != nullptr);
			ROBOTICK_ASSERT_MSG(byte_count % sizeof(T) == 0, "DynamicStructStorageVector::set_bytes() byte_count must be divisible by sizeof(T)");
			set(static_cast<const T*>(src), byte_count / sizeof(T));
		}

		T& operator[](size_t index)
		{
			ROBOTICK_ASSERT_MSG(index < capacity(), "DynamicStructStorageVector::operator[] index beyond capacity [%zu/%zu]", index, capacity());
			ROBOTICK_ASSERT_MSG(index < size(), "DynamicStructStorageVector::operator[] index beyond size [%zu/%zu]", index, size());
			return data()[index];
		}

		const T& operator[](size_t index) const
		{
			ROBOTICK_ASSERT_MSG(index < capacity(), "DynamicStructStorageVector::operator[] index beyond capacity [%zu/%zu]", index, capacity());
			ROBOTICK_ASSERT_MSG(index < size(), "DynamicStructStorageVector::operator[] index beyond size [%zu/%zu]", index, size());
			return data()[index];
		}

		T* begin() { return data(); }
		T* end() { return data() + count; }
		const T* begin() const { return data(); }
		const T* end() const { return data() + count; }

		T* data()
		{
			assert_descriptor_initialized("data()");
			return static_cast<T*>(field_descriptors[0].get_data_ptr(this));
		}

		const T* data() const
		{
			assert_descriptor_initialized("data() const");
			return static_cast<const T*>(field_descriptors[0].get_data_ptr(const_cast<SelfType*>(this)));
		}

		const StructDescriptor& get_struct_descriptor() const
		{
			assert_descriptor_initialized("get_struct_descriptor()");
			return struct_descriptor;
		}

		static const StructDescriptor* resolve_descriptor(const void* instance)
		{
			const SelfType* self = static_cast<const SelfType*>(instance);
			if (!self)
			{
				return nullptr;
			}

			self->assert_descriptor_initialized("resolve_descriptor()");
			return &(self->struct_descriptor);
		}

		static bool plan_storage(const void* instance, DynamicStructStoragePlan& out_plan)
		{
			const SelfType* self = static_cast<const SelfType*>(instance);
			if (!self)
			{
				return false;
			}

			self->assert_descriptor_initialized("plan_storage()");
			ROBOTICK_ASSERT_MSG(
				self->capacity_value > 0, "DynamicStructStorageVector::plan_storage() requires initialize_capacity() before engine binding");
			ROBOTICK_ASSERT_MSG(sizeof(T) > 0, "DynamicStructStorageVector::plan_storage() does not support zero-sized element types");
			ROBOTICK_ASSERT_MSG(self->capacity_value <= (SIZE_MAX / sizeof(T)),
				"DynamicStructStorageVector::plan_storage() size overflow for capacity %u and element size %zu",
				self->capacity_value,
				sizeof(T));

			out_plan.size_bytes = static_cast<size_t>(self->capacity_value) * sizeof(T);
			out_plan.alignment = alignof(T);
			return true;
		}

		static bool bind_storage(
			void* instance, const WorkloadsBuffer& workloads_buffer, size_t storage_offset_in_workloads_buffer, size_t storage_size_bytes)
		{
			SelfType* self = static_cast<SelfType*>(instance);
			if (!self)
			{
				return false;
			}

			self->assert_descriptor_initialized("bind_storage()");
			const size_t expected_size = static_cast<size_t>(self->capacity_value) * sizeof(T);
			ROBOTICK_ASSERT_MSG(storage_size_bytes == expected_size,
				"DynamicStructStorageVector::bind_storage() expected %zu bytes, received %zu bytes",
				expected_size,
				storage_size_bytes);

			const uint8_t* storage_base = workloads_buffer.raw_ptr();
			const uint8_t* self_ptr = reinterpret_cast<const uint8_t*>(self);
			ROBOTICK_ASSERT_MSG(storage_base != nullptr, "DynamicStructStorageVector::bind_storage() requires valid WorkloadsBuffer storage");
			ROBOTICK_ASSERT_MSG(
				self_ptr >= storage_base, "DynamicStructStorageVector::bind_storage() instance pointer precedes WorkloadsBuffer base");

			const size_t self_offset_in_workloads_buffer = static_cast<size_t>(self_ptr - storage_base);
			ROBOTICK_ASSERT_MSG(storage_offset_in_workloads_buffer >= self_offset_in_workloads_buffer,
				"DynamicStructStorageVector::bind_storage() storage offset %zu underflows instance offset %zu",
				storage_offset_in_workloads_buffer,
				self_offset_in_workloads_buffer);

			self->field_descriptors[0].offset_within_container = storage_offset_in_workloads_buffer - self_offset_in_workloads_buffer;
			self->refresh_descriptor_runtime_fields();
			self->count = 0;
			return true;
		}

	  private:
		static TypeId make_element_type_id()
		{
			TypeId result{"DynamicStructStorageVectorElement"};
			result.value = ElementTypeIdValue;
			return result;
		}

		void initialize_descriptor()
		{
			ROBOTICK_ASSERT_MSG(
				!descriptor_initialized, "DynamicStructStorageVector::initialize_capacity() called more than once on the same instance");

			field_descriptors[0] = FieldDescriptor{"data_buffer", make_element_type_id(), OFFSET_UNBOUND, capacity_value};
			field_descriptors[1] = FieldDescriptor{"count", GET_TYPE_ID(uint32_t), offsetof(SelfType, count), 1};
			field_descriptors[2] = FieldDescriptor{"capacity", GET_TYPE_ID(uint32_t), offsetof(SelfType, capacity_value), 1};
			struct_descriptor.fields.use(field_descriptors, 3);
			descriptor_initialized = true;
		}

		void assert_descriptor_initialized(const char* context) const
		{
			ROBOTICK_ASSERT_MSG(
				descriptor_initialized, "DynamicStructStorageVector::%s requires initialize_capacity() before descriptor/data access", context);
		}

		void refresh_descriptor_runtime_fields()
		{
			assert_descriptor_initialized("refresh_descriptor_runtime_fields()");
			field_descriptors[0].element_count = capacity_value;
			field_descriptors[1].offset_within_container = offsetof(SelfType, count);
			field_descriptors[2].offset_within_container = offsetof(SelfType, capacity_value);
		}

	  private:
		FieldDescriptor field_descriptors[3];
		StructDescriptor struct_descriptor;
		uint32_t count = 0;
		uint32_t capacity_value = 0;
		bool descriptor_initialized = false;
	};
} // namespace robotick
