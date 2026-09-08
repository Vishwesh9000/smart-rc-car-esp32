#pragma once

#include "Arduino.h"
#include "../Logger/LogLevels.h"
#include <string>
#include <string_view>

class Timer {
    public:
    static constexpr std::string_view tag = "TIMER";
    static inline LogLevel initialLogLevel = LogLevel::VERBOSE, defaultTimerLogLevel = LogLevel::DEBUG;
    LogLevel objectLogLevel, timerLogLevel;

    const std::string objectName;
    std::string funcName;
    static inline bool initDone = false;
    bool objTimer = false, funcTimer = false;
    uint32_t startTimeMS, startTimeUS;

    explicit Timer(std::string_view name = {}, LogLevel level = LogLevel::_DEFAULT);
    ~Timer();
    static void init();
    void start(std::string_view name = {}, LogLevel level = LogLevel::_DEFAULT);
    uint32_t recordCheckpoint(std::string_view name1 = "Checkpoint", LogLevel level = LogLevel::_DEFAULT);
    uint32_t end();
};