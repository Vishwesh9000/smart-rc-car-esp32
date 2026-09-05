//Logger.cpp
#include "Arduino.h"
#include "Logger.h"

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::addTag(std::string_view tag, LogLevel level) {
    if (level == LogLevel::_DEFAULT) level = globalLogLevel;

    if (tagLevels.find(tag) != tagLevels.end()) log(_tag, LogLevel::ERROR, "Tag \"%.*s\" already exists", (int)tag.length(), tag.data());

    tagLevels.insert_or_assign(std::string(tag), level);
}

void Logger::setLevel(LogLevel level, std::string_view tag) {
    if (tag.empty()) {
        globalLogLevel = level;
        return;
    }
    
    auto it = tagLevels.find(tag);
    if (it == tagLevels.end()) {
        log(_tag, LogLevel::FATAL, "Can't set tag level: Tag \"%.*s\" does not exist", (int)tag.length(), tag.data());
    }
    it->second = level;
}

void Logger::log(std::string_view tag, LogLevel level, const char* msg, ...) {
    if (!shouldLog(tag, level)) return;

    va_list args;
    va_start(args, msg);

    LogMessage logMsg = {.timestamp = millis(), .level = level, .value = std::nullopt};
    int written = vsnprintf(logMsg.msg, sizeof(logMsg.msg), msg, args);
    va_end(args);
    snprintfErrorLogger(written, sizeof(logMsg.msg), LogLevel::FATAL, "vsnprintf() Failed", "vsnprintf() Failed: Buffer overflowed");

    size_t copyLen = std::min(tag.length(), sizeof(logMsg.tag));
    memcpy(logMsg.tag, tag.data(), copyLen);
    logMsg.tag[copyLen] = '\0';

    BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
    if (flag != pdPASS) log(_tag, LogLevel::FATAL, "Sending log message through queue failed");

    if (level == LogLevel::FATAL) vTaskDelay(pdMS_TO_TICKS(10'000));
}




void Logger::logMem(const std::string_view tag, LogLevel level, MemFlag flags, const char* msg, ...) {
    if (!shouldLog(tag, level)) return;
    
    LogMessage logMsg = {.timestamp = millis(), .level = level, .value = std::nullopt};

    size_t copyLen = std::min(tag.length(), sizeof(logMsg.tag)-1);
    memcpy(logMsg.tag, tag.data(), copyLen);
    logMsg.tag[copyLen] = '\0';

    char *curPos = logMsg.msg, *endPos = &logMsg.msg[sizeof(logMsg.msg)];

    constexpr std::string_view prefix = "Memory Stats: (";
    if (prefix.length() < sizeof(logMsg.msg)) memcpy(logMsg.msg, prefix.data(), prefix.length());
    else log(_tag, LogLevel::FATAL, "logMem() Failed(1): Buffer overflow");
    curPos += prefix.length();

    va_list args;
    va_start(args, msg);
    int written = vsnprintf(curPos, endPos-curPos, msg, args);
    va_end(args);
    snprintfErrorLogger(written, endPos-curPos, LogLevel::FATAL, "logMem() Failed(2): vsnprintf() Error", "logMem() Failed(2): Buffer Overflow");
    curPos += written;

    if (endPos-curPos < 4) log(_tag, LogLevel::FATAL, "logMem() Failed(3): Buffer Overflowed");
    *(curPos++) = ')';
    *(curPos++) = ':';
    *(curPos++) = '\n';
    *curPos = '\0';
    
    BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
    if (flag != pdPASS) log(_tag, LogLevel::FATAL, "logMem() Failed: Failed to send through queue");
    curPos = logMsg.msg;

    bool printBlockSize = ((flags & MemFlag::BLOCK) != 0);
    if ((flags & MemFlag::INTERNAL) != 0) {
        written = snprintf(curPos, endPos-curPos, "\tINTERNAL RAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        snprintfErrorLogger(written, endPos-curPos, LogLevel::FATAL, "logMem() Failed(4): snprintf() Error", "logMem() Failed(4): Buffer Overflowed");
        curPos += written;
        if (printBlockSize) {
            written = snprintf(curPos, endPos-curPos, " (Block: %lu)", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            snprintfErrorLogger(written, endPos-curPos, LogLevel::FATAL, "logMem() Failed(5): snprintf() Error", "logMem() Failed(5): Buffer Overflowed");
        }

        BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) log(_tag, LogLevel::FATAL, "logMem() Failed: Failed to send through queue");
        curPos = logMsg.msg;
    }

    if ((flags & MemFlag::SPI_RAM) != 0) {
        written = snprintf(curPos, endPos-curPos, "\tSPI RAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        snprintfErrorLogger(written, endPos-curPos, LogLevel::FATAL, "logMem() Failed(7): snprintf() Error", "logMem() Failed(7): Buffer Overflowed");
        curPos += written;
        if (printBlockSize) {
            written = snprintf(curPos, endPos-curPos, " (Block: %lu)", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
            snprintfErrorLogger(written, endPos-curPos, LogLevel::FATAL, "logMem() Failed(8): snprintf() Error", "logMem() Failed(8): Buffer Overflowed");
            curPos += written;
        }

        BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) log(_tag, LogLevel::FATAL, "logMem() Failed: Failed to send through queue");
        curPos = logMsg.msg;
    }


    if (level == LogLevel::FATAL) vTaskDelay(pdMS_TO_TICKS(10'000));
}

constexpr char Logger::toChar(const LogLevel level) {
    switch (level) {
        case LogLevel::_DEFAULT:
            return ' ';
        case LogLevel::NONE:
            return ' ';
        case LogLevel::FATAL:
            return 'F';
        case LogLevel::ERROR:
            return 'E';
        case LogLevel::WARNING:
            return 'W';
        case LogLevel::INFO:
            return 'I';
        case LogLevel::DEBUG:
            return 'D';
    }
    return 'Z';
}

Logger::Logger() {
    queue = xQueueCreate(10, sizeof(LogMessage));
    configASSERT(queue != nullptr);
    BaseType_t ok = xTaskCreate(
        taskFunction,
        "logger_task",
        stackSize,
        this,
        priority,
        &task
    );
    configASSERT(ok == pdPASS);
}


bool Logger::shouldLog(std::string_view tag, LogLevel level) {
    auto it = tagLevels.find(tag);
    if (it == tagLevels.end()) log(_tag, LogLevel::FATAL, "Tag \"%.*s\" doesn't exist", (int)tag.length(), tag.data());
    if (level <= it->second) return true;
    else return false;
}

void Logger::logTask() {
    LogMessage logMsg{};
    for (;;) {
        BaseType_t flag = xQueueReceive(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) {
            Serial.println("logTask() Error: Error recieving from queue");
            vTaskDelay(pdMS_TO_TICKS(1'000));
            continue;
        }
        
        if (logMsg.level == LogLevel::FATAL) Serial.print(ANSI_MAGENTA);
        if (logMsg.level == LogLevel::ERROR) Serial.print(ANSI_RED);
        if (logMsg.level == LogLevel::WARNING) Serial.print(ANSI_YELLOW);

        Serial.printf("%6u[LOG][%c][%s]: ", logMsg.timestamp, toChar(logMsg.level), logMsg.tag);
        Serial.println(logMsg.msg);
        Serial.print(ANSI_RESET);
        if (logMsg.level == LogLevel::FATAL) {
            ESP.restart();
        }
    }
}

void Logger::taskFunction(void *logObj) {
    static_cast<Logger*>(logObj)->logTask();
}

void Logger::snprintfErrorLogger(int written, size_t bufferSize, LogLevel level, const char *failedError, const char *overflowError) {
    if (written < 0) log(_tag, level, failedError);
    else if (written >= bufferSize) log(_tag, level, overflowError);
}


