// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

// This header is intended for **commonly used includes** that are relevant to **any workload**.
// It should be kept minimal — only include essentials that are broadly useful across most workloads,
// such as tick metadata, fixed strings, and field/registration helpers.
//
// Avoid pulling in anything heavy, platform-specific, or unrelated to **general** workload logic.

#include "robotick/api_base.h"

#if defined(_WIN32)
#if defined(ROBOTICK_EXPORTS)
#define ROBOTICK_API __declspec(dllexport)
#else
#define ROBOTICK_API __declspec(dllimport)
#endif
#else
#define ROBOTICK_API
#endif

#include "robotick/framework/Engine.h"
#include "robotick/framework/TickInfo.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/data/State.h"
#include "robotick/framework/math/Vec2.h"
#include "robotick/framework/math/Vec3.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"
