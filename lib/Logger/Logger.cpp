//Logger.cpp
#include "Arduino.h"
#include "Logger.h"
#include <inttypes.h>

//TODO: Add tracking of dropped log messages

void IRAM_ATTR Logger::LogMessage::assignTagAndMessage(const char* tag, const char* msg) {
    size_t copyLen;
    if (tag != nullptr) {
        copyLen = strnlen(tag, sizeof(this->tag)-1);
        memcpy(this->tag, tag, copyLen);
        this->tag[copyLen] = '\0';
    }
    if (msg != nullptr) {
        copyLen = strnlen(msg, sizeof(this->msg)-1);
        memcpy(this->msg, msg, copyLen);
        this->msg[copyLen] = '\0';
    }
}

Logger *Logger::_instance = nullptr;

void Logger::init() {
    static Logger logger;
    _instance = &logger;
}

Logger& Logger::instance() {
    return *_instance;
}

void Logger::addTag(std::string_view tag, LogLevel level) {
    if (level == LogLevel::_DEFAULT) level = defaultLogLevel;

    if (tagLevels.find(tag) != tagLevels.end()) LOG_E(this->tag, "Tag \"%.*s\" already exists", (int)tag.length(), tag.data());

    tagLevels.insert_or_assign(std::string(tag), level);
}

void Logger::setLevel(LogLevel level, std::string_view tag) {
    auto it = tagLevels.find(tag);
    if (it == tagLevels.end()) {
        LOG_F(this->tag, "Can't set tag level: Tag \"%.*s\" does not exist", (int)tag.length(), tag.data());
    }
    it->second = level;
}

void Logger::log(std::string_view tag, LogLevel level, const char* msg, ...) {
    if (!shouldLog(tag, level)) return;

    LogMessage logMsg = {.timestamp = esp_timer_get_time(), .level = level, .value = std::nullopt};

    va_list args;
    va_start(args, msg);
    int written = vsnprintf(logMsg.msg, sizeof(logMsg.msg), msg, args);
    va_end(args);
    snprintfErrorLogger<LogLevel::FATAL, LogLevel::ERROR>(written, sizeof(logMsg.msg), "vsnprintf() Failed", "Log buffer overflowed");

    size_t copyLen = std::min<size_t>(tag.length(), sizeof(logMsg.tag));
    memcpy(logMsg.tag, tag.data(), copyLen);
    logMsg.tag[copyLen] = '\0';

    if (xTaskGetCurrentTaskHandle() != task) {
        BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) LOG_F(this->tag, "Sending log message through queue failed");
        if (level == LogLevel::FATAL) {
            handleFatalError();
        }
    }
    else {
        printLogMessage(&logMsg);
        if (level == LogLevel::FATAL) abort(); 
    }
}

void IRAM_ATTR Logger::logFromISR(LogLevel level, LogMessage &logMsg, BaseType_t &higherPriorityTaskWoken) {
    logMsg.timestamp = esp_timer_get_time();
    logMsg.level = level;
    logMsg.tag[sizeof(logMsg.tag)-1] = '\0';
    logMsg.msg[sizeof(logMsg.msg)-1] = '\0';
    xQueueSendFromISR(queue, &logMsg, &higherPriorityTaskWoken);
}

void Logger::logMem(const std::string_view tag, LogLevel level, MemFlag flags, const char* msg, ...) {
    if (!shouldLog(tag, level)) return;
    
    LogMessage logMsg = {.timestamp = esp_timer_get_time(), .level = level, .value = std::nullopt};

    size_t copyLen = std::min<size_t>(tag.length(), sizeof(logMsg.tag)-1);
    memcpy(logMsg.tag, tag.data(), copyLen);
    logMsg.tag[copyLen] = '\0';

    char *curPos = logMsg.msg;
    const char *endPos = logMsg.msg + sizeof(logMsg.msg);
    constexpr std::string_view prefix = "Memory Stats (";
    if constexpr (prefix.length() < sizeof(logMsg.msg)) memcpy(logMsg.msg, prefix.data(), prefix.length());
    else {
        LOG_E(this->tag, "Log buffer overflowed");
        logMsg.msg[sizeof(logMsg.msg) - 1] = '\0';
    }
    curPos += prefix.length();

    int written;
    if (curPos < endPos) {
        va_list args;
        va_start(args, msg);
        written = vsnprintf(curPos, endPos-curPos, msg, args);
        va_end(args);
        
        snprintfErrorLogger<LogLevel::FATAL, LogLevel::ERROR>(written, endPos-curPos, "vsnprintf() Failed", "Log buffer overflowed");
        curPos += written;
    }
    if (curPos >= endPos) {}
    else if (endPos-curPos < 4) LOG_E(this->tag, "Buffer Overflowed");
    else {
        *(curPos++) = ')';
        *(curPos++) = ':';
        *(curPos++) = '\n';
        *curPos = '\0';
    }
    
    logMsg.msg[sizeof(logMsg.msg)-2] = '\n';
    logMsg.msg[sizeof(logMsg.msg)-1] = '\0';

    BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
    if (flag != pdPASS) LOG_F(this->tag, "logMem() Failed: Failed to send through queue");
    curPos = logMsg.msg;

    bool printBlockSize = ((flags & MemFlag::BLOCK) != 0);
    if ((flags & MemFlag::INTERNAL) != 0) {
        written = snprintf(curPos, endPos-curPos, "\tINTERNAL RAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        snprintfErrorLogger<LogLevel::FATAL, LogLevel::ERROR>(written, endPos-curPos, "snprintf() Failed", "Log buffer overflowed");
        curPos += written;
        if (printBlockSize && curPos < endPos) {
            written = snprintf(curPos, endPos-curPos, " (Block: %lu)", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            snprintfErrorLogger<LogLevel::FATAL, LogLevel::ERROR>(written, endPos-curPos, "snprintf() Failed", "Log buffer overflowed");
        }

        BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) LOG_F(this->tag, "logMem() Failed: Failed to send through queue: %s", tag.data());
        curPos = logMsg.msg;
    }

    if ((flags & MemFlag::SPI_RAM) != 0) {
        written = snprintf(curPos, endPos-curPos, "\tSPI RAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        snprintfErrorLogger<LogLevel::FATAL, LogLevel::ERROR>(written, endPos-curPos, "snprintf() Failed", "Log buffer overflowed");
        curPos += written;
        if (printBlockSize && curPos < endPos) {
            written = snprintf(curPos, endPos-curPos, " (Block: %lu)", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
            snprintfErrorLogger<LogLevel::FATAL, LogLevel::ERROR>(written, endPos-curPos, "snprintf() Failed", "Log buffer overflowed");
            curPos += written;
        }

        BaseType_t flag = xQueueSend(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) LOG_F(this->tag, "logMem() Failed: Failed to send through queue: %s", tag.data());
        curPos = logMsg.msg;
    }


    if (level == LogLevel::FATAL) handleFatalError();
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
        case LogLevel::VERBOSE:
            return 'V';
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

    assert(tagLevels.find(tag) != tagLevels.end() && tagLevels.find(taskTag) != tagLevels.end());
}

bool Logger::shouldLog(std::string_view tag, LogLevel level) {
    auto it = tagLevels.find(tag);
    if (it == tagLevels.end()) LOG_F(this->tag, "Tag \"%.*s\" doesn't exist", (int)tag.length(), tag.data());
    if (level <= it->second) return true;
    else return false;
}

void Logger::logTask() {
    LogMessage logMsg{};
    for (;;) {
        BaseType_t flag = xQueueReceive(queue, &logMsg, portMAX_DELAY);
        if (flag != pdPASS) {
            Serial.write(ANSI_RED "9999999999[LOG][E][LOGGER_TASK]: logTask() Error: Error recieving from queue\n" ANSI_RESET);
            continue;
        }
        // Timer _t("printLogMsg", LogLevel::VERBOSE);
        printLogMessage(&logMsg);
        if (logMsg.level == LogLevel::FATAL) {
            Serial.write(ANSI_MAGENTA "9999999999[LOG][F][LOGGER_TASK]: FATAL Errors shouldn't propogate to logTask()\n" ANSI_RESET);
            abort();
        }
    }
}

void Logger::taskFunction(void *logObj) {
    static_cast<Logger*>(logObj)->logTask();
}

bool Logger::printLogMessage(LogMessage *logMsg) {
    //TODO: Add std::optional value support

    // if (logMsg.level == LogLevel::FATAL) Serial.print(ANSI_MAGENTA);
    // if (logMsg.level == LogLevel::ERROR) Serial.print(ANSI_RED);
    // if (logMsg.level == LogLevel::WARNING) Serial.print(ANSI_YELLOW);

    // Serial.printf("%6u[LOG][%c][%s]: ", logMsg.timestamp, toChar(logMsg.level), logMsg.tag);
    // Serial.println(logMsg.msg);
    // Serial.print(ANSI_RESET);
    // if (logMsg.level == LogLevel::FATAL) {
    //     ESP.restart();
    // }
    // Timer _t("printLogMsg", LogLevel::VERBOSE);
    const char* colorCode = "";
    if (logMsg->level == LogLevel::FATAL) colorCode = ANSI_MAGENTA;
    else if (logMsg->level == LogLevel::ERROR) colorCode = ANSI_RED;
    else if (logMsg->level == LogLevel::WARNING) colorCode = ANSI_YELLOW;

    // _t.recordCheckpoint("Color Codes");

    char buffer[40 + sizeof(logMsg->tag) + sizeof(logMsg->msg) + 10]; //10 is extra padding
    int written = snprintf(buffer, sizeof(buffer), "%s%10" PRId64 "[LOG][%c][%s]: %s\n" ANSI_RESET, 
                                                    colorCode, logMsg->timestamp/1000, toChar(logMsg->level), logMsg->tag, logMsg->msg);

    // _t.recordCheckpoint("snprintf()");

    if (written < 0) {
        Serial.write(ANSI_RED "9999999999[LOG][E][LOGGER]: printLogMesage() Error: snprintf() Failed\n" ANSI_RESET);
        return false;
    }
    if (static_cast<long long>(written) < static_cast<long long>(sizeof(buffer))) Serial.write(buffer, written);
    else {
        Serial.write(buffer, sizeof(buffer)-1);
        Serial.write(ANSI_RED "... [TRUNCATED]" ANSI_RESET);
    }
    // _t.recordCheckpoint("End");
    return true;
}

void Logger::handleFatalError() {
    portDISABLE_INTERRUPTS();
    LogMessage logMsg;
    BaseType_t higherPriorityTaskWoken;
    while (xQueueReceiveFromISR(queue, &logMsg, &higherPriorityTaskWoken) == pdPASS) {
        printLogMessage(&logMsg);
        if (logMsg.level == LogLevel::FATAL) {
            abort();
        }
    }
    Serial.write(ANSI_MAGENTA "9999999999[LOG][E][LOGGER_TASK]: Flushing logs before terminating failed" ANSI_RESET);
    abort();
}






