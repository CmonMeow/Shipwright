#pragma once

// Compatibility surface required by the retained UDP transport.
// It deliberately exposes only the Windows primitives and logger that transport uses.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>

#include <runtime/log/Log.h>
