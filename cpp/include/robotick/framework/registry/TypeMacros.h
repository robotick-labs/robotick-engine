// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/WorkloadTypeHelpers.h"

namespace robotick
{
	/// @brief AutoRegisterType - type-registration helper common to all the below macros
	struct AutoRegisterType
	{
		explicit AutoRegisterType(const TypeDescriptor& desc);
	};
} // namespace robotick

/// @brief Macro to register Primitives:
#define ROBOTICK_REGISTER_PRIMITIVE(TypeName, ToStringFn, FromStringFn)                                                                              \
	static_assert(                                                                                                                                   \
		std::is_standard_layout<TypeName>::value, #TypeName " is not standard layout. Only standard layout types can be registered as primitives."); \
	static_assert(std::is_trivially_copyable<TypeName>::value,                                                                                       \
		#TypeName " is not trivially copyable. Only trivially copyable items can be registered as primitive types.");                                \
	static constexpr ::robotick::TypeDescriptor s_type_desc_##TypeName = {                                                                           \
		#TypeName, GET_TYPE_ID(TypeName), sizeof(TypeName), alignof(TypeName), ::robotick::TypeCategory::Primitive, {}, ToStringFn, FromStringFn};   \
	static const ::robotick::AutoRegisterType s_auto_register_##TypeName(s_type_desc_##TypeName);

/// @brief Macros to register Structs:
#define ROBOTICK_REGISTER_STRUCT_BEGIN(StructType) static ::robotick::FieldDescriptor s_fields_##StructType[] = {

#define ROBOTICK_STRUCT_FIELD(StructType, FieldType, FieldName) {#FieldName, GET_TYPE_ID(FieldType), offsetof(StructType, FieldName)},

#define ROBOTICK_REGISTER_STRUCT_END(StructType)                                                                                                     \
	}                                                                                                                                                \
	;                                                                                                                                                \
	static_assert(std::is_standard_layout<StructType>::value,                                                                                        \
		#StructType " is not standard layout. Only standard layout structs can be registered as field types.");                                      \
	static_assert(std::is_trivially_copyable<StructType>::value,                                                                                     \
		#StructType " is not trivially copyable. Only trivially copyable structs can be registered as field types.");                                \
	static constexpr ::robotick::StructDescriptor s_struct_desc_##StructType = {::robotick::ArrayView<::robotick::FieldDescriptor>{                  \
		s_fields_##StructType, sizeof(s_fields_##StructType) / sizeof(::robotick::FieldDescriptor)}};                                                \
	static constexpr ::robotick::TypeDescriptor s_type_desc_##StructType = {#StructType,                                                             \
		GET_TYPE_ID(StructType),                                                                                                                     \
		sizeof(StructType),                                                                                                                          \
		alignof(StructType),                                                                                                                         \
		::robotick::TypeCategory::Struct,                                                                                                            \
		{&s_struct_desc_##StructType},                                                                                                               \
		nullptr,                                                                                                                                     \
		nullptr};                                                                                                                                    \
	static const ::robotick::AutoRegisterType s_register_##StructType(s_type_desc_##StructType);

/// @brief Macro to register Dynamic Structs :
#define ROBOTICK_REGISTER_DYNAMIC_STRUCT(TypeName, ResolveFn)                                                                                        \
	static_assert(std::is_standard_layout<TypeName>::value,                                                                                          \
		#TypeName " is not standard layout. Only standard layout types can be registered as dynamic structs.");                                      \
	static_assert(std::is_trivially_copyable<TypeName>::value,                                                                                       \
		#TypeName " is not trivially copyable. Only trivially copyable types can be registered as dynamic structs.");                                \
	static const ::robotick::DynamicStructDescriptor s_dynamic_struct_desc_##TypeName = {ResolveFn};                                                 \
	static const ::robotick::TypeDescriptor s_type_desc_##TypeName = {#TypeName,                                                                     \
		GET_TYPE_ID(TypeName),                                                                                                                       \
		sizeof(TypeName),                                                                                                                            \
		alignof(TypeName),                                                                                                                           \
		::robotick::TypeCategory::DynamicStruct,                                                                                                     \
		{&s_dynamic_struct_desc_##TypeName},                                                                                                         \
		nullptr,                                                                                                                                     \
		nullptr};                                                                                                                                    \
	static const ::robotick::AutoRegisterType s_register_##TypeName(s_type_desc_##TypeName);

#define ROBOTICK_SUPPRESS_UNUSED_WARNING_START                                                                                                       \
	_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") _Pragma("GCC diagnostic ignored \"-Wattributes\"")

#define ROBOTICK_SUPPRESS_UNUSED_WARNING_END _Pragma("GCC diagnostic pop")

/// @brief Macros to register Workloads:
#define ROBOTICK_REGISTER_WORKLOAD_BASE(WorkloadTypeName, ConfigTypePtr, InputTypePtr, OutputTypePtr)                                                \
	ROBOTICK_SUPPRESS_UNUSED_WARNING_START                                                                                                           \
	static_assert(                                                                                                                                   \
		std::is_standard_layout<WorkloadTypeName>::value, #WorkloadTypeName " is not standard layout. All workloads must be standard layout.");      \
	static const ::robotick::WorkloadDescriptor s_workload_desc_##WorkloadTypeName =                                                                 \
		::robotick::registry::make_workload_descriptor<WorkloadTypeName>(ConfigTypePtr, InputTypePtr, OutputTypePtr);                                \
	static const ::robotick::TypeDescriptor s_type_desc_##WorkloadTypeName = {#WorkloadTypeName,                                                     \
		GET_TYPE_ID(WorkloadTypeName),                                                                                                               \
		sizeof(WorkloadTypeName),                                                                                                                    \
		alignof(WorkloadTypeName),                                                                                                                   \
		::robotick::TypeCategory::Workload,                                                                                                          \
		{&s_workload_desc_##WorkloadTypeName},                                                                                                       \
		nullptr,                                                                                                                                     \
		nullptr};                                                                                                                                    \
	static const ::robotick::AutoRegisterType s_register_##WorkloadTypeName(s_type_desc_##WorkloadTypeName);                                         \
	volatile bool g_##WorkloadTypeName##_NoDeadStrip = false;                                                                                        \
	ROBOTICK_SUPPRESS_UNUSED_WARNING_END

#define ROBOTICK_REGISTER_WORKLOAD_1(Type) ROBOTICK_REGISTER_WORKLOAD_BASE(Type, nullptr, nullptr, nullptr)
#define ROBOTICK_REGISTER_WORKLOAD_2(Type, Config) ROBOTICK_REGISTER_WORKLOAD_BASE(Type, &s_type_desc_##Config, nullptr, nullptr)
#define ROBOTICK_REGISTER_WORKLOAD_3(Type, Config, Inputs)                                                                                           \
	ROBOTICK_REGISTER_WORKLOAD_BASE(Type, &robotick::s_type_desc_##Config, &s_type_desc_##Inputs, nullptr)
#define ROBOTICK_REGISTER_WORKLOAD_4(Type, Config, Inputs, Outputs)                                                                                  \
	ROBOTICK_REGISTER_WORKLOAD_BASE(Type, &robotick::s_type_desc_##Config, &s_type_desc_##Inputs, &s_type_desc_##Outputs)

#define GET_ROBOTICK_REGISTER_WORKLOAD_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define ROBOTICK_REGISTER_WORKLOAD(...)                                                                                                              \
	GET_ROBOTICK_REGISTER_WORKLOAD_MACRO(                                                                                                            \
		__VA_ARGS__, ROBOTICK_REGISTER_WORKLOAD_4, ROBOTICK_REGISTER_WORKLOAD_3, ROBOTICK_REGISTER_WORKLOAD_2, ROBOTICK_REGISTER_WORKLOAD_1)         \
	(__VA_ARGS__)

#define ROBOTICK_KEEP_WORKLOAD(WorkloadTypeName)                                                                                                     \
	extern volatile bool g_##WorkloadTypeName##_NoDeadStrip;                                                                                         \
	g_##WorkloadTypeName##_NoDeadStrip = true;
