// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#define ROBOTICK_MODEL_CONFIG_ENTRIES(...) static constexpr robotick::FieldConfigEntry __VA_ARGS__

#define ROBOTICK_MODEL_WORKLOAD(name, type_str, rate, ...)                                                                                           \
	static constexpr robotick::WorkloadSeed name = robotick::WorkloadSeed(robotick::GET_TYPE_ID(type_str), #name, rate, nullptr, 0, __VA_ARGS__)

#define ROBOTICK_MODEL_WORKLOAD_WITH_CHILDREN(name, type_str, rate, child_arr, num_children)                                                         \
	static constexpr robotick::WorkloadSeed name = robotick::WorkloadSeed(robotick::GET_TYPE_ID(type_str), #name, rate, child_arr, num_children)