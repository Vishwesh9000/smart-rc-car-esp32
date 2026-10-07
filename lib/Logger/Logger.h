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
#include <optional>

#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL static_cast<int>(LogLevel::VERBOSE)
#endif

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
    public:
    static constexpr std::string_view tag = "LOGGER", taskTag = "LOGGER_TASK";
    LogLevel defaultLogLevel = LogLevel::VERBOSE;
    static constexpr LogLevel logCompileLevel = static_cast<LogLevel>(LOG_COMPILE_LEVEL);

    static constexpr size_t logBufferSize=120, tagSize=16;
    static constexpr UBaseType_t maxQueueMessages = 10, priority = 5;
    static constexpr size_t stackSize = 4096;

    struct LogMessage {
        int64_t timestamp; // In microseconds
        LogLevel level;
        char tag[tagSize];
        char msg[logBufferSize];
        std::optional<double> value;
        void assignTagAndMessage(const char* tag, const char* msg);
    };
    
    public:
    static void init();
    static Logger& instance();
    void addTag(std::string_view tag, LogLevel level);
    void setLevel(LogLevel level, std::string_view tag);
    void log(std::string_view tag, LogLevel level, const char* msg, ...);
    void logFromISR(LogLevel level, LogMessage& logMsg, BaseType_t& higherPriorityTaskWoken);
    void logMem(std::string_view tag, LogLevel level, MemFlag flags, const char* msg, ...);
    constexpr char toChar(LogLevel level);

    private:
    static Logger* _instance;
    TaskHandle_t task;
    QueueHandle_t queue;

    static constexpr LogLevel initialInternalLogLevel = LogLevel::VERBOSE, initialLogTaskLogLevel = LogLevel::VERBOSE;
    std::map<std::string, LogLevel, std::less<>> tagLevels = {{std::string(tag), initialInternalLogLevel}, {std::string(taskTag), initialLogTaskLogLevel}};

    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    bool shouldLog(std::string_view tag, LogLevel level);
    void logTask();
    static void taskFunction(void *logObj);
    template<LogLevel errorLevel, LogLevel overflowLevel>
    void snprintfErrorLogger(int written, size_t bufferSize, const char *failedError, const char *overflowError);
    bool printLogMessage(LogMessage *logMsg);
    void handleFatalError();
};

#define LOG_AT(tag, level, msg, ...)                                \
    do {                                                            \
        if constexpr (level <= Logger::logCompileLevel) {           \
            Logger::instance().log(tag, level, msg, ##__VA_ARGS__); \
        }                                                           \
    } while (0)

#define LOG_F(tag, msg, ...) LOG_AT(tag, LogLevel::FATAL, msg, ##__VA_ARGS__)
#define LOG_E(tag, msg, ...) LOG_AT(tag, LogLevel::ERROR, msg, ##__VA_ARGS__)
#define LOG_W(tag, msg, ...) LOG_AT(tag, LogLevel::WARNING, msg, ##__VA_ARGS__)
#define LOG_I(tag, msg, ...) LOG_AT(tag, LogLevel::INFO, msg, ##__VA_ARGS__)
#define LOG_D(tag, msg, ...) LOG_AT(tag, LogLevel::DEBUG, msg, ##__VA_ARGS__)
#define LOG_V(tag, msg, ...) LOG_AT(tag, LogLevel::VERBOSE, msg, ##__VA_ARGS__)

#define LOG_MEM_AT(tag, level, flags, msg, ...)                                 \
    do {                                                                        \
        if constexpr (level <= Logger::logCompileLevel) {                       \
            Logger::instance().logMem(tag, level, flags, msg, ##__VA_ARGS__);   \
        }                                                                       \
    } while (0)                                                                 \

#define LOG_MEM_F(tag, flags, msg, ...) LOG_MEM_AT(tag, LogLevel::FATAL, flags, msg, ##__VA_ARGS__)
#define LOG_MEM_E(tag, flags, msg, ...) LOG_MEM_AT(tag, LogLevel::ERROR, flags, msg, ##__VA_ARGS__)
#define LOG_MEM_W(tag, flags, msg, ...) LOG_MEM_AT(tag, LogLevel::WARNING, flags, msg, ##__VA_ARGS__)
#define LOG_MEM_I(tag, flags, msg, ...) LOG_MEM_AT(tag, LogLevel::INFO, flags, msg, ##__VA_ARGS__)
#define LOG_MEM_D(tag, flags, msg, ...) LOG_MEM_AT(tag, LogLevel::DEBUG, flags, msg, ##__VA_ARGS__)
#define LOG_MEM_V(tag, flags, msg, ...) LOG_MEM_AT(tag, LogLevel::VERBOSE, flags, msg, ##__VA_ARGS__)

#define LOG_ISR_AT(level, logMsg, higherPriorityTaskWoken)                                               \
    do {                                                                        \
        if constexpr (level <= Logger::logCompileLevel) {                       \
            Logger::instance().logFromISR(level, logMsg, higherPriorityTaskWoken);                       \
        }                                                                       \
    } while (0)


#define LOG_ISR_F(logMsg, higherPriorityTaskWoken) LOG_ISR_AT(LogLevel::FATAL, logMsg, higherPriorityTaskWoken)
#define LOG_ISR_E(logMsg, higherPriorityTaskWoken) LOG_ISR_AT(LogLevel::ERROR, logMsg, higherPriorityTaskWoken)
#define LOG_ISR_W(logMsg, higherPriorityTaskWoken) LOG_ISR_AT(LogLevel::WARNING, logMsg, higherPriorityTaskWoken)
#define LOG_ISR_I(logMsg, higherPriorityTaskWoken) LOG_ISR_AT(LogLevel::INFO, logMsg, higherPriorityTaskWoken)
#define LOG_ISR_D(logMsg, higherPriorityTaskWoken) LOG_ISR_AT(LogLevel::DEBUG, logMsg, higherPriorityTaskWoken)
#define LOG_ISR_V(logMsg, higherPriorityTaskWoken) LOG_ISR_AT(LogLevel::VERBOSE, logMsg, higherPriorityTaskWoken)

// TODO: Reduce repetetiveness of reg, mem, and ISR macros
// #define LOG_COMPILE_TIME_CHECK(level, loggerFunc)                               \
//     do {                                                                        \
//         if constexpr (level <= Logger::logCompileLevel) {                       \
//             Logger::instance().loggerFunc                                       \
//         }                                                                       \
//     } while (0)

// #define LOG_ISR


#define LOG_ADD_TAG(tag, level) Logger::instance().addTag(tag, level)
#define LOG_SET_LEVEL(level, tag) Logger::instance().setLevel(level, tag)


#include "Logger.tpp"