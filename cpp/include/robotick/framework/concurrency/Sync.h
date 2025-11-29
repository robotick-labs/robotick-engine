// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/memory/StdApproved.h"

#include <condition_variable>
#include <mutex>

namespace robotick
{
	using Mutex = std_approved::mutex;
	using ConditionVariable = std_approved::condition_variable;
	using LockGuard = std_approved::lock_guard<Mutex>;
	using UniqueLock = std_approved::unique_lock<Mutex>;
} // namespace robotick
