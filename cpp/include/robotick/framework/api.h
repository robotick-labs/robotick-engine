// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(_WIN32)
#if defined(ROBOTICK_EXPORTS)
#define ROBOTICK_API __declspec(dllexport)
#else
#define ROBOTICK_API __declspec(dllimport)
#endif
#else
#define ROBOTICK_API
#endif
