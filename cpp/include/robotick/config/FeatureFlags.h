// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "robotick/config/PlatformDefines.h"

//
// Feature toggles based on platform
//

#if defined(ROBOTICK_PLATFORM_UBUNTU)
#define ROBOTICK_ENABLE_EXCEPTIONS 1
#define ROBOTICK_ENABLE_RTTI 1
#define ROBOTICK_ENABLE_PYTHON 1

#elif defined(ROBOTICK_PLATFORM_ESP32)
#define ROBOTICK_ENABLE_EXCEPTIONS 0
#define ROBOTICK_ENABLE_RTTI 0
#define ROBOTICK_ENABLE_PYTHON 0

#elif defined(ROBOTICK_PLATFORM_EMBEDDED)
#define ROBOTICK_ENABLE_EXCEPTIONS 0
#define ROBOTICK_ENABLE_RTTI 0
#define ROBOTICK_ENABLE_PYTHON 0

#else
#error "Unrecognized platform — please define ROBOTICK_PLATFORM_XXX in PlatformDefines.h"
#endif
