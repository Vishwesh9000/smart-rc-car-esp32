#pragma once

enum class LogLevel: int {
    _DEFAULT = -1,
    NONE,
    FATAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    VERBOSE
};