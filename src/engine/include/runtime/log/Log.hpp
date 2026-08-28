#pragma once

#include "Log.h"

#include <fmt/format.h>
#include <fmt/std.h>

#include <string>
#include <utility>

inline void WriteLog(const char* message) {
    Error("%s", message ? message : "");
}

inline void WriteLog(const std::string& message) {
    Error("%s", message.c_str());
}

template <typename... Args>
inline void WriteLog(fmt::format_string<Args...> format, Args&&... args) {
    const std::string message = fmt::format(format, std::forward<Args>(args)...);
    Error("%s", message.c_str());
}
