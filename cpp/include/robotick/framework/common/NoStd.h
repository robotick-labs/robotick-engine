#pragma once

// Robotick STL guard.
//
// The core engine/workload runtime must remain deterministic on MCUs, so we
// forbid direct std:: usage there.  This is because std containers pull in dynamic allocation,
// exceptions, and platform-specific behaviors that fight our contiguous-buffer
// design. Platform shims (Threading, networking, host tooling) may opt-in by
// defining ROBOTICK_ALLOW_STD before including this file; everywhere else, any
// attempt to use std:: will trigger a compile error once this header is wired
// into the common prelude.

#if !defined(ROBOTICK_ALLOW_STD)
#if !defined(ROBOTICK_STD_FORBIDDEN)
#define ROBOTICK_STD_FORBIDDEN 1
#define ROBOTICK_STD_USAGE_BANNED__SEE_robotick_framework_common_NoStd_h
#define std ROBOTICK_STD_USAGE_BANNED__SEE_robotick_framework_common_NoStd_h
#endif
#endif
