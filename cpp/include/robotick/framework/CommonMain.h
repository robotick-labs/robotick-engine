// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Shared includes for launcher-generated main files. Only use this when the consumer
// is a generated entrypoint that needs the core engine pieces and signal helpers;
// keep the header small so that it cannot easily pull in unrelated functionality.
//
// It exposes the minimal set of includes that a launcher main needs without becoming
// a grab bag of unrelated framework APIs.

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/system/EntryPoint.h"
#include "robotick/framework/system/Signals.h"
