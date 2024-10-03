#pragma once

#define HAS_FLAG(var, flag) (((var) & (flag)) == (flag))

#ifndef WIN32
void DebugBreak(void);
#endif