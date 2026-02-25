// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

// robotick/framework/system/System.h

#pragma once

namespace robotick
{

	class System
	{
	  public:
		/// Perform platform-specific system initialization (e.g. board init hooks, signal handlers, etc)
		static void initialize();
	};

} // namespace robotick
