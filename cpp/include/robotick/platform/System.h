// robotick/platform/System.h

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
