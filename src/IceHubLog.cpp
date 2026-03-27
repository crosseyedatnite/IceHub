#include "IceHubLog.h"
#include <stdarg.h>
#include <stdio.h>
#ifdef USE_DYNAMIC_LOGS
#include <time.h>
#include <sys/time.h>
#endif

#ifdef USE_DYNAMIC_LOGS
IceHubLog::IceHubLog(size_t maxEntries)
    : _head(0), _count(0), _currentLineLen(0), _lastPrintTime(0),
      _streamTargetId(255), _streamTimeoutTime(0), _streamCallback(nullptr), _streamCallbackContext(nullptr)
{
    _maxEntries = maxEntries;
    _dynamicLogBuffer = new DynamicLogEntry[_maxEntries];
    for (size_t i = 0; i < _maxEntries; i++) {
        _dynamicLogBuffer[i].message[0] = '\0';
        _dynamicLogBuffer[i].count = 0;
    }
    memset(_currentLine, 0, sizeof(_currentLine));
}
#else
IceHubLog::IceHubLog()
    : _head(0), _count(0), _currentLineLen(0), _lastPrintTime(0),
      _streamTargetId(255), _streamTimeoutTime(0), _streamCallback(nullptr), _streamCallbackContext(nullptr)
{
    _maxEntries = MAX_LOG_ENTRIES; // Safely bounded
    memset(_logBuffer, 0, sizeof(_logBuffer));
    memset(_currentLine, 0, sizeof(_currentLine));
}
#endif

IceHubLog::~IceHubLog() {
#ifdef USE_DYNAMIC_LOGS
    delete[] _dynamicLogBuffer;
#endif
}

#ifdef __AVR__
static int ice_putchar(char c, FILE *stream) {
    extern IceHubLog iceLog;
    if (c == '\n') {
        iceLog.println("");
    } else if (c != '\r') {
        char buf[2] = {c, '\0'};
        iceLog.print(buf);
    }
    return 0;
}
static FILE ice_stdout;
#endif

void IceHubLog::setup() {
#ifdef __AVR__
    fdev_setup_stream(&ice_stdout, ice_putchar, NULL, _FDEV_SETUP_WRITE);
    stdout = &ice_stdout;
#endif
    println(F("IceHubLog: Service Initialized."));
}

void IceHubLog::loop() {
    if (_currentLineLen > 0 && (millis() - _lastPrintTime > PRINT_TIMEOUT_MS)) {
        flushCurrentLine();
    }
}

void IceHubLog::print(const char* msg) {
    Serial.print(msg);
    if (_currentLineLen >= MAX_LINE_LENGTH - 1) _currentLineLen = 0; // Recover from corruption
    size_t msgLen = strlen(msg);
    size_t spaceLeft = (MAX_LINE_LENGTH - 1) - _currentLineLen;
    size_t copyLen = (msgLen > spaceLeft) ? spaceLeft : msgLen;

    if (copyLen > 0) {
        memcpy(_currentLine + _currentLineLen, msg, copyLen);
        _currentLineLen += copyLen;
        _currentLine[_currentLineLen] = '\0';
    }
    _lastPrintTime = millis();
}

void IceHubLog::print(const __FlashStringHelper* msg) {
    Serial.print(msg);
    if (_currentLineLen >= MAX_LINE_LENGTH - 1) _currentLineLen = 0; // Recover from corruption
    PGM_P p = reinterpret_cast<PGM_P>(msg);
    size_t msgLen = strlen_P(p);
    size_t spaceLeft = (MAX_LINE_LENGTH - 1) - _currentLineLen;
    size_t copyLen = (msgLen > spaceLeft) ? spaceLeft : msgLen;

    if (copyLen > 0) {
        memcpy_P(_currentLine + _currentLineLen, p, copyLen);
        _currentLineLen += copyLen;
        _currentLine[_currentLineLen] = '\0';
    }
    _lastPrintTime = millis();
}

void IceHubLog::println(const __FlashStringHelper* msg) {
    Serial.println(msg);
    if (_currentLineLen >= MAX_LINE_LENGTH - 1) _currentLineLen = 0; // Recover from corruption
    PGM_P p = reinterpret_cast<PGM_P>(msg);
    size_t msgLen = strlen_P(p);
    size_t spaceLeft = (MAX_LINE_LENGTH - 1) - _currentLineLen;
    size_t copyLen = (msgLen > spaceLeft) ? spaceLeft : msgLen;

    if (copyLen > 0) {
        memcpy_P(_currentLine + _currentLineLen, p, copyLen);
        _currentLineLen += copyLen;
        _currentLine[_currentLineLen] = '\0';
    }
    flushCurrentLine();
}

void IceHubLog::println(const char* msg) {
    Serial.println(msg);
    if (_currentLineLen >= MAX_LINE_LENGTH - 1) _currentLineLen = 0; // Recover from corruption
    size_t msgLen = strlen(msg);
    size_t spaceLeft = (MAX_LINE_LENGTH - 1) - _currentLineLen;
    size_t copyLen = (msgLen > spaceLeft) ? spaceLeft : msgLen;

    if (copyLen > 0) {
        memcpy(_currentLine + _currentLineLen, msg, copyLen);
        _currentLineLen += copyLen;
        _currentLine[_currentLineLen] = '\0';
    }
    flushCurrentLine();
}

void IceHubLog::printf(const char* format, ...) {
    char buf[MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        println(buf);
    } else {
        print(buf);
    }
}

void IceHubLog::printf(const __FlashStringHelper* format, ...) {
    char buf[MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
#ifdef __AVR__
    vsnprintf_P(buf, sizeof(buf), reinterpret_cast<PGM_P>(format), args);
#else
    vsnprintf(buf, sizeof(buf), reinterpret_cast<const char*>(format), args);
#endif
    va_end(args);
    
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        println(buf);
    } else {
        print(buf);
    }
}

void IceHubLog::flushCurrentLine() {
    if (_currentLineLen == 0) return;
    addLogEntry(_currentLine);
    _currentLineLen = 0;
    _currentLine[0] = '\0';
}

bool IceHubLog::setLogStreamTarget(uint8_t targetId, uint32_t durationMs) {
    bool isNewLease = (millis() > _streamTimeoutTime || _streamTargetId != targetId);
    _streamTargetId = targetId;
    _streamTimeoutTime = millis() + durationMs;
    return isNewLease;
}

void IceHubLog::setLogStreamCallback(LogStreamCallback cb, void* context) {
    _streamCallback = cb;
    _streamCallbackContext = context;
}

void IceHubLog::addLogEntry(const char* entry) {
#ifdef USE_DYNAMIC_LOGS
    unsigned long now = millis();
    if (_count > 0) {
        size_t latestIndex = (_maxEntries + _head - 1) % _maxEntries;
        if (strcmp(_dynamicLogBuffer[latestIndex].message, entry) == 0) {
            _dynamicLogBuffer[latestIndex].count++;
            _dynamicLogBuffer[latestIndex].lastTimestamp = now;
            return; // Skip adding duplicate entries, just roll up
        }
    }

    strncpy(_dynamicLogBuffer[_head].message, entry, MAX_LINE_LENGTH - 1);
    _dynamicLogBuffer[_head].message[MAX_LINE_LENGTH - 1] = '\0';
    _dynamicLogBuffer[_head].firstTimestamp = now;
    _dynamicLogBuffer[_head].lastTimestamp = now;
    _dynamicLogBuffer[_head].count = 1;
    
    _head = (_head + 1) % _maxEntries;
    if (_count < _maxEntries) {
        _count++;
    }
#else
    if (_count > 0 && strcmp(getLogEntry(0), entry) == 0) {
        return; // Skip adding duplicate entries to the history buffer
    }

    strncpy(_logBuffer[_head], entry, MAX_LINE_LENGTH - 1);
    _logBuffer[_head][MAX_LINE_LENGTH - 1] = '\0';
    _head = (_head + 1) % _maxEntries;
    if (_count < _maxEntries) {
        _count++;
    }
#endif

    // Active streaming if lease is valid
    if (_streamCallback && _streamTimeoutTime > 0 && millis() <= _streamTimeoutTime) {
        _streamCallback(entry, _streamTargetId, _streamCallbackContext);
    }
}

const char* IceHubLog::getLogEntry(size_t offsetFromLatest) const {
    if (offsetFromLatest >= _count) return "";
    size_t index = (_maxEntries + _head - 1 - offsetFromLatest) % _maxEntries;
#ifdef USE_DYNAMIC_LOGS
    return _dynamicLogBuffer[index].message;
#else
    return _logBuffer[index];
#endif
}

#ifdef USE_DYNAMIC_LOGS
String IceHubLog::getLogEntryFormatted(size_t offsetFromLatest) const {
    if (offsetFromLatest >= _count) return String("");
    size_t index = (_maxEntries + _head - 1 - offsetFromLatest) % _maxEntries;
    const DynamicLogEntry& entry = _dynamicLogBuffer[index];
    
    unsigned long ms = entry.firstTimestamp;
    
    char tsBuf[64];
    time_t nowTime;
    time(&nowTime);
    struct tm timeinfo;
    
    // If year is greater than 2020, we assume NTP has successfully synchronized
    if (nowTime > 1577836800) { 
        unsigned long diffMs = millis() - ms; // Difference in time since log occurred
        time_t entryTime = nowTime - (diffMs / 1000);
        localtime_r(&entryTime, &timeinfo);
        strftime(tsBuf, sizeof(tsBuf), "[%Y-%m-%d %H:%M:%S] ", &timeinfo);
    } else {
        unsigned long sec = ms / 1000;
        unsigned long min = sec / 60;
        unsigned long hr = min / 60;
        snprintf(tsBuf, sizeof(tsBuf), "[%02lu:%02lu:%02lu.%03lu] ", hr, min % 60, sec % 60, ms % 1000);
    }
    
    String out = String(tsBuf) + entry.message;
    if (entry.count > 1) {
        unsigned long lms = entry.lastTimestamp;
        char rollBuf[128];
        
        if (nowTime > 1577836800) {
            unsigned long diffLms = millis() - lms;
            time_t lastTime = nowTime - (diffLms / 1000);
            localtime_r(&lastTime, &timeinfo);
            char lastTimeStr[32];
            strftime(lastTimeStr, sizeof(lastTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            snprintf(rollBuf, sizeof(rollBuf), " (Repeated %zu times, last at %s)", entry.count, lastTimeStr);
        } else {
            unsigned long lsec = lms / 1000;
            unsigned long lmin = lsec / 60;
            unsigned long lhr = lmin / 60;
            snprintf(rollBuf, sizeof(rollBuf), " (Repeated %zu times, last at %02lu:%02lu:%02lu.%03lu)", 
                entry.count, lhr, lmin % 60, lsec % 60, lms % 1000);
        }
        out += rollBuf;
    }
    return out;
}
#endif

size_t IceHubLog::getCurrentLogCount() const {
    return _count;
}