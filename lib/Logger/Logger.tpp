
template<LogLevel errorLevel, LogLevel overflowLevel>
void Logger::snprintfErrorLogger(int written, size_t bufferSize, const char *failedError, const char *overflowError) {
    if (written < 0) LOG_AT(this->tag, errorLevel, failedError);
    else if (written >= bufferSize) LOG_AT(tag, overflowLevel, overflowError);
}
