#pragma once

#define HAS_FLAG(var, flag) (((var) & (flag)) == (flag))

/* Stops in the debugger in debug builds. Does nothing in release builds, where the Win32
   DebugBreak() would kill the game without a debugger attached. */
void AtDebugBreak();
