#include "Arduino.h"
#include "Logging.h"
#include <string_view>

class Timer {
    public:
    static inline std::string_view tag = "TIMER";
    static inline LogLevel initialLogLevel = LogLevel::VERBOSE, defaultTimerLogLevel = LogLevel::DEBUG;
    LogLevel objectLogLevel, timerLogLevel;

    const std::string_view objectName;
    std::string_view funcName;
    static inline bool initDone = false;
    bool objTimer = false, funcTimer = false;
    uint32_t startTime;

    explicit Timer(std::string_view name = {}, LogLevel level = LogLevel::_DEFAULT);
    ~Timer();
    static void init();
    void start(std::string_view name = {}, LogLevel level = LogLevel::_DEFAULT);
    uint32_t recordCheckpoint(std::string_view name1 = {}, LogLevel level = LogLevel::_DEFAULT);
    uint32_t end();
};