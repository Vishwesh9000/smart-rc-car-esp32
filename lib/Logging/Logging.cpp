//Logging.cpp
#include "Arduino.h"
#include "Logging.h"


Logging::Logging() {}

Logging& Logging::instance() {
    static Logging logger;
    return logger;
}
bool Logging::shouldLog(std::string_view tag, LogLevel level) {
    auto it = tagLevels.find(tag);
    if (it == tagLevels.end()) log(LOGGER, LogLevel::FATAL, "Tag \"%.*s\" doesn't exist", (int)tag.length(), tag.data());
    if (level <= it->second) return true;
    else return false;
}

void Logging::addTag(std::string_view tag, LogLevel level) {
    if (level == LogLevel::_DEFAULT) level = globalLogLevel;

    if (tagLevels.find(tag) != tagLevels.end()) log(LOGGER, LogLevel::ERROR, "Tag \"%.*s\" already exists", (int)tag.length(), tag.data());

    tagLevels.insert_or_assign(std::string(tag), level);
}

void Logging::setLevel(LogLevel level, std::string_view tag) {
    if (tag.empty()) {
        globalLogLevel = level;
        return;
    }
    
    auto it = tagLevels.find(tag);
    if (it == tagLevels.end()) {
        log(LOGGER, LogLevel::FATAL, "Can't set tag level: Tag \"%.*s\" does not exist", (int)tag.length(), tag.data());
    }
    it->second = level;
}

//Always Logs (No level checks)
void Logging::log(std::string_view tag, LogLevel level, const char* msg, va_list args) {
    char buffer[logBufferSize];
    size_t remaining = logBufferSize;
    int written;
    const char* prefix = "[LOG][%c][%.*s]: ";

    written = snprintf(buffer, remaining, prefix, toChar(level), (int)tag.length(), tag.data());
    if (written < 0) log(LOGGER, LogLevel::FATAL, "vsnprintf() Failed");
    else if (written >= remaining) log(LOGGER, LogLevel::FATAL, "vsnprintf() Failed: Buffer overflowed");
    remaining -= written;

    written = vsnprintf(buffer+written, remaining, msg, args);
    if (written < 0) log(LOGGER, LogLevel::FATAL, "vsnprintf() Failed");
    else if (written >= remaining) log(LOGGER, LogLevel::FATAL, "vsnprintf() Failed: Buffer overflowed");
    
    if (level == LogLevel::ERROR) Serial.print(ANSI_RED);
    else if (level == LogLevel::WARNING) Serial.print(ANSI_YELLOW);
    Serial.println(buffer);
    Serial.print(ANSI_RESET);
}

void Logging::log(std::string_view tag, LogLevel level, const char* msg, ...) {
    if (!shouldLog(tag, level)) return;

    va_list args;
    va_start(args, msg);
    log(tag, level, msg, args);
    va_end(args);
    if (level == LogLevel::FATAL) {
        delay(20000);
        ESP.restart();
    }
}

void Logging::logMem(const std::string_view tag, LogLevel level, MemFlag flags, const char* msg, ...) {
    if (!shouldLog(tag, level)) return;
    char buffer[logBufferSize];
    const char* memMsg = "Memory Stats: (%s)";
    int written = snprintf(buffer, logBufferSize, memMsg, msg);
    if (written < 0) log(LOGGER, LogLevel::FATAL, "snprintf() failed");
    else if (written >= logBufferSize) log(LOGGER, LogLevel::FATAL, "snprintf() Failed: Buffer overflowed");
    va_list args;
    va_start(args, msg);
    log(tag, level, buffer);
    va_end(args);

    bool printBlockSize = ((flags & MemFlag::BLOCK) != 0);
    if ((flags & MemFlag::INTERNAL) != 0) {
        Serial.printf("\tINTERNAL RAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        if (printBlockSize) Serial.printf(" (Largest Block: %lu)\n", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        else Serial.println();     
    }
    if ((flags & MemFlag::SPI_RAM) != 0) {
        Serial.printf("\tSPI RAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        if (printBlockSize) Serial.printf(" (Largest Block: %lu)\n", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        else Serial.println();    
    }
    Serial.println();

}

constexpr char Logging::toChar(const LogLevel level) {
    switch (level) {
        case LogLevel::_DEFAULT:
            log(LOGGER, LogLevel::FATAL, "Can't pass LogLevel::_DEFAULT into toString()");
        case LogLevel::NONE:
            log(LOGGER, LogLevel::FATAL, "Can't pass LogLevel::NONE into toString()");
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
