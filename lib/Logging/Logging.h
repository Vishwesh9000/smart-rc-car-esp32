//Logging.h
#pragma once
#include <map>
#include <string>
#include <string_view>
#include <stdarg.h>
#include <type_traits>

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


struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
};

enum class LogLevel {
    _DEFAULT = -1,
    NONE,
    FATAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    VERBOSE
};

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

class Logging {
    private:
    const std::string_view LOGGER = "LOGGER";
    LogLevel globalLogLevel = LogLevel::INFO;
    std::map<std::string, LogLevel, std::less<>> tagLevels = {{std::string(LOGGER), LogLevel::INFO}};
    static constexpr size_t logBufferSize=64;

    Logging(const Logging&) = delete;
    Logging(Logging&&) = delete;
    Logging& operator=(const Logging&) = delete;
    Logging& operator=(Logging&&) = delete;

    bool shouldLog(std::string_view tag, LogLevel level);
    void log(std::string_view tag, LogLevel level, const char* msg, va_list args);

    public:
    Logging();
    static Logging& instance();
    void addTag(std::string_view tag, LogLevel level = LogLevel::_DEFAULT);
    void setLevel(LogLevel level, std::string_view tag = std::string_view{});
    void log(std::string_view tag, LogLevel level, const char* msg, ...);
    void logMem(std::string_view tag, LogLevel level, MemFlag flags, const char* msg, ...);
    constexpr char toChar(LogLevel level);
};



