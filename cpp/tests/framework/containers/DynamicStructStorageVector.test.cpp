// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/containers/DynamicStructStorageVector.h"
#include "robotick/config/AssertUtils.h"
#include "robotick/framework/registry/TypeMacros.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		using DynamicUint32Buffer = DynamicStructStorageVector<uint32_t, GET_TYPE_ID(uint32_t).value>;
		ROBOTICK_REGISTER_DYNAMIC_STRUCT(
			DynamicUint32Buffer, DynamicUint32Buffer::resolve_descriptor, DynamicUint32Buffer::plan_storage, DynamicUint32Buffer::bind_storage)

		static size_t align_offset(size_t offset, size_t alignment)
		{
			ROBOTICK_ASSERT_MSG(alignment > 0, "align_offset() requires alignment > 0");
			const size_t remainder = offset % alignment;
			return (remainder == 0) ? offset : (offset + (alignment - remainder));
		}
	} // namespace

	TEST_CASE("Unit/Framework/Containers/DynamicStructStorageVector")
	{
		SECTION("Exposes a dynamic struct descriptor with data_buffer, count, and capacity")
		{
			DynamicUint32Buffer buffer;
			buffer.initialize_capacity(4);

			const StructDescriptor* struct_desc = DynamicUint32Buffer::resolve_descriptor(&buffer);
			REQUIRE(struct_desc != nullptr);
			REQUIRE(struct_desc->fields.size() == 3);

			CHECK(struct_desc->fields[0].name == "data_buffer");
			CHECK(struct_desc->fields[0].type_id == GET_TYPE_ID(uint32_t));
			CHECK(struct_desc->fields[0].element_count == 4);
			CHECK(struct_desc->fields[1].name == "count");
			CHECK(struct_desc->fields[2].name == "capacity");
		}

		SECTION("Requires binding before data access")
		{
			DynamicUint32Buffer buffer;
			buffer.initialize_capacity(4);
			ROBOTICK_REQUIRE_ERROR_MSG(buffer.data(), "has not yet been bound");
		}

		SECTION("Requires initialize_capacity before descriptor access")
		{
			DynamicUint32Buffer buffer;
			ROBOTICK_REQUIRE_ERROR_MSG(DynamicUint32Buffer::resolve_descriptor(&buffer), "requires initialize_capacity");
		}

		SECTION("Rejects repeated initialize_capacity on the same instance")
		{
			DynamicUint32Buffer buffer;
			buffer.initialize_capacity(4);
			ROBOTICK_REQUIRE_ERROR_MSG(buffer.initialize_capacity(4), "called more than once on the same instance");
		}

		SECTION("Plans storage and binds into WorkloadsBuffer-backed storage")
		{
			DynamicStructStoragePlan plan;
			DynamicUint32Buffer temp;
			temp.initialize_capacity(4);
			REQUIRE(DynamicUint32Buffer::plan_storage(&temp, plan));
			CHECK(plan.size_bytes == sizeof(uint32_t) * 4);
			CHECK(plan.alignment == alignof(uint32_t));

			const size_t storage_offset = align_offset(sizeof(DynamicUint32Buffer), plan.alignment);
			WorkloadsBuffer workloads_buffer(storage_offset + plan.size_bytes);
			DynamicUint32Buffer* buffer = workloads_buffer.as<DynamicUint32Buffer>(0);
			new (buffer) DynamicUint32Buffer();
			buffer->initialize_capacity(4);

			REQUIRE(DynamicUint32Buffer::bind_storage(buffer, workloads_buffer, storage_offset, plan.size_bytes));
			REQUIRE(buffer->capacity() == 4);
			REQUIRE(buffer->size() == 0);

			buffer->add(11);
			buffer->add(22);
			CHECK(buffer->size() == 2);
			CHECK(buffer->data()[0] == 11);
			CHECK(buffer->data()[1] == 22);

			uint32_t values[] = {5, 6, 7, 8};
			buffer->set(values, 4);
			CHECK(buffer->size() == 4);
			CHECK(buffer->full());
			CHECK(buffer->operator[](3) == 8);
		}
	}
} // namespace robotick::test
