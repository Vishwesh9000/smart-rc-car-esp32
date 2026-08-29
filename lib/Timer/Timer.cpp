#include "Timer.h"

static Logging& logger = Logging::instance();

Timer::Timer(std::string_view name, LogLevel level): 
        objectLogLevel((level == LogLevel::_DEFAULT ? defaultTimerLogLevel : level)), 
        timerLogLevel(objectLogLevel), objectName(name), startTime(millis()) {
    if (!initDone) init();
    objTimer = true;
    startTime = millis();
}

Timer::~Timer() {
    if (objTimer) {
        uint32_t endTime = millis();
        std::string_view name;
        if (!objectName.empty()) name = objectName;
        else name = "Timer";
        logger.log(tag, timerLogLevel, "%.*s: %lums", static_cast<int>(name.length()), name.data(), static_cast<unsigned long>(endTime-startTime));
    }
}

void Timer::init() {
    if (!initDone) {
        logger.addTag(tag, initialLogLevel);
        initDone = true;
    }
}
void Timer::start(std::string_view name, LogLevel level) {
    if (funcTimer) logger.log(tag, LogLevel::WARNING, "Timer started twice before calling end()");
    if (level != LogLevel::_DEFAULT) timerLogLevel = level;
    else timerLogLevel = objectLogLevel;

    funcName = name;
    objTimer = false;
    funcTimer = true;
    startTime = millis();
}

uint32_t Timer::recordCheckpoint(std::string_view name1, LogLevel level) {
    if (!(funcTimer || objTimer)) {
        logger.log(tag, LogLevel::ERROR, "Can't record checkpoint: No timer running");
        return 0;
    }
    uint32_t endTime = millis();

    if (level == LogLevel::_DEFAULT) level = timerLogLevel;

    std::string_view name;
    if (!funcName.empty()) name = funcName;
    else if (!objectName.empty()) name = objectName;
    else name = "Timer";
    
    if (name1.empty()) logger.log(tag, level, "%.*s: (Checkpoint) %lums", static_cast<int>(name.length()), name.data(), static_cast<unsigned long>(endTime));
    else logger.log(tag, level, "%.*s: (%.*s) %lums", static_cast<int>(name.length()), name.data(), static_cast<unsigned long>(endTime-startTime));
    return endTime-startTime;
}

uint32_t Timer::end() {
    if (!funcTimer) {
        logger.log(tag, LogLevel::ERROR, "Timer::end() not executed: Timer not started");
        return 0;
    }
    uint32_t endTime = millis();
    std::string_view name;
    if (!funcName.empty()) name = funcName;
    else if (!objectName.empty()) name = objectName;
    else name = "Timer";
    logger.log(tag, timerLogLevel, "%.*s: %lums", static_cast<int>(name.length()), name.data(), static_cast<unsigned long>(endTime-startTime));
    funcTimer = false;
    return endTime-startTime;
}