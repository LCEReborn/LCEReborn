#ifndef PLATFORM_PC_LOG_HPP
#define PLATFORM_PC_LOG_HPP

#include <cstdarg>
#include <cstddef>
#include <string>

enum class LogLevel : unsigned char
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

#ifndef LCE_LOG_MIN_LEVEL
#define LCE_LOG_MIN_LEVEL 1
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LCE_LOG_PRINTF(formatIndex, firstArgument) __attribute__((format(printf, formatIndex, firstArgument)))
#else
#define LCE_LOG_PRINTF(formatIndex, firstArgument)
#endif

class Logger
{
public:
    template <std::size_t Length>
    explicit constexpr Logger(const char (&name)[Length]) : mName(name)
    {
    }

    const char* Name() const { return mName; }

    void Write(LogLevel level, const char* format, ...) const LCE_LOG_PRINTF(3, 4);
    void WriteV(LogLevel level, const char* format, va_list arguments) const;
    void WriteWideV(LogLevel level, const wchar_t* format, va_list arguments) const;
    void WriteText(LogLevel level, const std::string& text) const;

private:
    const char* mName;
};

#define LCE_LOG(logger, level, ...)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (static_cast<int>(level) >= LCE_LOG_MIN_LEVEL)                                                              \
            (logger).Write(level, __VA_ARGS__);                                                                        \
    } while (0)

#define LOG_TRACE(logger, ...) LCE_LOG(logger, LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(logger, ...) LCE_LOG(logger, LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(logger, ...) LCE_LOG(logger, LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(logger, ...) LCE_LOG(logger, LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(logger, ...) LCE_LOG(logger, LogLevel::Error, __VA_ARGS__)
#define LOG_FATAL(logger, ...) LCE_LOG(logger, LogLevel::Fatal, __VA_ARGS__)

namespace Log
{
void Start(const std::string& directory);
void Flush();
void Stop();
}

#endif
