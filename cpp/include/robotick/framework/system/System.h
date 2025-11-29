// robotick/framework/system/System.h

// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace robotick
{

	class System
	{
	  public:
		/// Perform platform-specific system initialization (e.g. M5.begin, signal handlers, etc)
		static void initialize();
	};

} // namespace robotick
