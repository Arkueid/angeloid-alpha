#pragma once
#include <cstdarg>  // va_list

// ──── Log level ────
enum class LogLevel { Debug, Info, Warn, Error };

// ──── Log function pointer ────
// Receives (level, module_tag, printf_fmt, va_args).
// Implementation is responsible for formatting prefix and trailing newline.
using LogFunc = void (*)(LogLevel level, const char* module,
                         const char* fmt, va_list args);

namespace mmd {
namespace detail {

inline LogFunc& logFuncStorage() {
    static LogFunc fn = nullptr;
    return fn;
}

}  // namespace detail

// Install a log function. Called by framework at init time.
inline void setLogFunc(LogFunc fn) {
    detail::logFuncStorage() = fn;
}

// Central dispatch — every macro routes through here.
// Does nothing if no LogFunc has been registered.
inline void logMessage(LogLevel level, const char* module,
                       const char* fmt, ...) {
    auto fn = detail::logFuncStorage();
    if (!fn) return;

    va_list args;
    va_start(args, fmt);
    fn(level, module, fmt, args);
    va_end(args);
}

}  // namespace mmd

// ──── Public macros — API unchanged ────
#define MMD_INFO(mod, fmt, ...)  \
    ::mmd::logMessage(::LogLevel::Info, mod, fmt, ##__VA_ARGS__)
#define MMD_WARN(mod, fmt, ...)  \
    ::mmd::logMessage(::LogLevel::Warn, mod, fmt, ##__VA_ARGS__)
#define MMD_ERROR(mod, fmt, ...) \
    ::mmd::logMessage(::LogLevel::Error, mod, fmt, ##__VA_ARGS__)

#ifdef MMD_DEBUG_ENABLED
#define MMD_DEBUG(mod, fmt, ...) \
    ::mmd::logMessage(::LogLevel::Debug, mod, fmt, ##__VA_ARGS__)
#else
#define MMD_DEBUG(mod, fmt, ...) ((void)0)
#endif
