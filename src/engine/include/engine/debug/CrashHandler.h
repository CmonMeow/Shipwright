#pragma once

#include <stddef.h>
#include <string>

#if (__linux__)
#include <csignal>
#include <cstdio>
#include <cxxabi.h> // for __cxa_demangle
#include <dlfcn.h>  // for dladdr
#include <execinfo.h>
#include <unistd.h>
#endif

#if _WIN32
#include <windows.h>
#endif

namespace Engine {
class CrashHandler {
  public:
    explicit CrashHandler(std::string applicationName);
    ~CrashHandler();

    static CrashHandler* GetActive();
    const std::string& GetApplicationName() const;
    void AppendLine(const char* str);
    void AppendStr(const char* str);
    void PrintCommon();

#ifdef __linux__
    void PrintRegisters(ucontext_t* ctx);
#elif _WIN32
    void PrintRegisters(CONTEXT* ctx);
    void PrintStack(CONTEXT* ctx);
#endif

  private:
    static CrashHandler* mActive;
    std::string mApplicationName;
    char* mOutBuffer = nullptr;
    static constexpr size_t gMaxBufferSize = 32768;
    size_t mOutBuffersize = 0;

    void AppendStrTrunc(const char* str);

    bool CheckStrLen(const char* str);
};
} // namespace Engine
