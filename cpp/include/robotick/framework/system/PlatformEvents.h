#pragma once

namespace robotick
{
	// Called once per tick to allow the platform to process any pending input/system messages
	void poll_platform_events();

	// Returns true if the platform has requested the application to exit (e.g. user closed window)
	bool should_exit_application();
} // namespace robotick
