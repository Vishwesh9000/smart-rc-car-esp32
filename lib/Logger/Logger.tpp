
template<LogLevel level>
void Logger::snprintfErrorLogger(int written, size_t bufferSize, const char *failedError, const char *overflowError) {
    if (written < 0) LOG_AT(tag, level, failedError);
    else if (written >= bufferSize) LOG_AT(tag, level, overflowError);
}