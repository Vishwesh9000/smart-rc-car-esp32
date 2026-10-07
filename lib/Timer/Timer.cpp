#include "Timer.h"
#include "../Logger/Logger.h"

// TODO: Add macro based timers
//TODO: Support Logger.h macros

Timer::Timer(std::string_view name, LogLevel level): 
        objectLogLevel((level == LogLevel::_DEFAULT ? defaultTimerLogLevel : level)), 
        timerLogLevel(objectLogLevel), objectName(name), startTimeMS(millis()), startTimeUS(micros()) {
    if (!initDone) init();
    objTimer = true;
}

Timer::~Timer() {
    if (objTimer) {
        uint32_t endTimeMS = millis(), endTimeUS = micros();
        std::string_view name;
        if (!objectName.empty()) name = objectName;
        else name = "Timer";
        bool printMicros = endTimeMS-startTimeMS < 100;
        Logger::instance().log(tag, timerLogLevel, "%.*s: %lu%s", static_cast<int>(name.length()), name.data(), 
                                        static_cast<unsigned long>(printMicros ? endTimeUS - startTimeUS : endTimeMS-startTimeMS), 
                                        printMicros ? "us" : "ms");
    }
}

void Timer::init() {
    if (!initDone) {
        Logger::init();
        Logger::instance().addTag(tag, initialLogLevel);
        initDone = true;
    }
}
void Timer::start(std::string_view name, LogLevel level) {
    if (funcTimer) LOG_W(tag, "Timer started twice before calling end()");
    if (level != LogLevel::_DEFAULT) timerLogLevel = level;
    else timerLogLevel = objectLogLevel;

    funcName = name;
    objTimer = false;
    funcTimer = true;
    startTimeMS = millis();
    startTimeUS = micros();
}

uint32_t Timer::recordCheckpoint(std::string_view checkPointName, LogLevel level) {
    if (!(funcTimer || objTimer)) {
        LOG_E(tag, "Can't record checkpoint: No timer running");
        return 0;
    }
    uint32_t timeDiffMS = millis() - startTimeMS, timeDiffUS = micros() - startTimeUS;

    if (level == LogLevel::_DEFAULT) level = timerLogLevel;

    std::string_view name;
    if (!funcName.empty()) name = funcName;
    else if (!objectName.empty()) name = objectName;
    else name = "Timer";
    
    bool printMicros = timeDiffMS < 100;
    Logger::instance().log(tag, level, "%.*s: (%.*s) %lu%s", static_cast<int>(name.length()), name.data(), static_cast<int>(checkPointName.length()), checkPointName.data(), 
                                                static_cast<unsigned long>(printMicros ? timeDiffUS : timeDiffMS),
                                                printMicros ? "us" : "ms");
    return timeDiffMS;
}

uint32_t Timer::end() {
    if (!funcTimer) {
        LOG_E(tag, "Timer::end() not executed: Timer not started");
        return 0;
    }
    uint32_t timeDiffMS = millis()-startTimeMS, timeDiffUS = micros()-startTimeUS;
    std::string_view name;
    if (!funcName.empty()) name = funcName;
    else if (!objectName.empty()) name = objectName;
    else name = "Timer";

    bool printMicros = timeDiffMS < 100;
    Logger::instance().log(tag, timerLogLevel, "%.*s: %lu%s", static_cast<int>(name.length()), name.data(), 
                                    static_cast<unsigned long>(printMicros ? timeDiffUS : timeDiffMS),
                                    printMicros ? "us" : "ms");
    funcTimer = false;
    return timeDiffMS;
}