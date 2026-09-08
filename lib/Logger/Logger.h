//Logger.h
#pragma once

#include "LogLevels.h"
#include "../Timer/Timer.h"

#include <map>
#include <string>
#include <string_view>
#include <stdarg.h>
#include <type_traits>
#include <algorithm>

#define ANSI_CODES //Comment out if monitor doesn't support the codes

#ifdef ANSI_CODES
#define ANSI_RESET      "\x1B[0m"
#define ANSI_RED        "\x1B[31m"
#define ANSI_GREEN      "\x1B[32m"
#define ANSI_YELLOW     "\x1B[33m"
#define ANSI_BLUE       "\x1B[34m"
#define ANSI_MAGENTA    "\x1B[35m"
#define ANSI_CYAN       "\x1B[36m"
#define ANSI_WHITE      "\x1B[37m"
#define ANSI_BOLD       "\x1B[1m"
#define ANSI_UNDERLINE  "\x1B[4m"
#else
#define ANSI_RESET      ""
#define ANSI_RED        ""
#define ANSI_GREEN      ""
#define ANSI_YELLOW     ""
#define ANSI_BLUE       ""
#define ANSI_MAGENTA    ""
#define ANSI_CYAN       ""
#define ANSI_WHITE      ""
#define ANSI_BOLD       ""
#define ANSI_UNDERLINE  ""
#endif

enum class MemFlag {
    INTERNAL = 1 << 0,
    SPI_RAM = 1 << 1,
    BLOCK = 1 << 2,
    ALL = 0b111
};

constexpr MemFlag operator|(MemFlag a, MemFlag b) {
    using U = std::underlying_type_t<MemFlag>;
    return static_cast<MemFlag>(static_cast<U>(a) | static_cast<U>(b));
}
constexpr MemFlag operator&(MemFlag a, MemFlag b) {
    using U = std::underlying_type_t<MemFlag>;
    return static_cast<MemFlag>(static_cast<U>(a) & static_cast<U>(b));
}
constexpr bool operator==(MemFlag f, int i) {
    using U = std::underlying_type_t<MemFlag>;
    return static_cast<U>(f) == i;
}
constexpr bool operator==(int i, MemFlag f) {
    return f==i;
}
constexpr bool operator!=(MemFlag f, int i) {
    return !(f==i);
}
constexpr bool operator!=(int i, MemFlag f) {
    return !(f==i);
}

class Logger {
    private:
    static constexpr std::string_view _tag = "LOGGER", _taskTag = "LOGGER_TASK";
    LogLevel defaultLogLevel = LogLevel::VERBOSE;
    static constexpr LogLevel initialInternalLogLevel = LogLevel::VERBOSE, initialLogTaskLogLevel = LogLevel::VERBOSE;
    static constexpr size_t logBufferSize=96, tagSize=16;
    static constexpr UBaseType_t maxQueueMessages = 10, priority = 5;
    static constexpr size_t stackSize = 4096;

    struct LogMessage {
        uint32_t timestamp;
        char tag[tagSize];
        char msg[logBufferSize];
        LogLevel level;
        std::optional<double> value;
    };
    
    public:
    static Logger& instance();
    void addTag(std::string_view tag, LogLevel level = LogLevel::_DEFAULT);
    void setLevel(LogLevel level, std::string_view tag = std::string_view{});
    void log(std::string_view tag, LogLevel level, const char* msg, ...);
    void logFromISR(LogMessage message);
    void logMem(std::string_view tag, LogLevel level, MemFlag flags, const char* msg, ...);
    constexpr char toChar(LogLevel level);

    private:
    TaskHandle_t task;
    QueueHandle_t queue;

    std::map<std::string, LogLevel, std::less<>> tagLevels = {{std::string(_tag), initialInternalLogLevel}, {std::string(_taskTag), initialLogTaskLogLevel}};

    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    bool shouldLog(std::string_view tag, LogLevel level);
    void logTask();
    static void taskFunction(void *logObj);
    void snprintfErrorLogger(int written, size_t bufferSize, LogLevel level, const char *failedError, const char *overflowError);
    bool printLogMessage(LogMessage *logMsg);
};



