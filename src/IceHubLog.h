#pragma once
#include <Arduino.h>

#include "ice_service.h"

class IceHubLog : public IceService {
public:
    typedef void (*LogStreamCallback)(const char* logLine, uint8_t targetId, void* context);

#ifdef USE_DYNAMIC_LOGS
    IceHubLog(size_t maxEntries = 50);
#else
    IceHubLog(); // Explicitly prevent large buffer assumptions on Nano
#endif
    ~IceHubLog();
    
    void print(const char* msg);
    void println(const char* msg);

    void print(const __FlashStringHelper* msg);
    void println(const __FlashStringHelper* msg);

    void printf(const char* format, ...);
    void printf(const __FlashStringHelper* format, ...);

    const char* getLogEntry(size_t offsetFromLatest) const;

#ifdef USE_DYNAMIC_LOGS
    String getLogEntryFormatted(size_t offsetFromLatest) const;
#endif

    size_t getCurrentLogCount() const;

    // Remote Log Streaming
    bool setLogStreamTarget(uint8_t targetId, uint32_t durationMs);
    void setLogStreamCallback(LogStreamCallback cb, void* context);

    // Service lifecycle
    void setup();
    void loop() override;

private:
    size_t _maxEntries;

#ifdef USE_DYNAMIC_LOGS
    static const size_t MAX_LINE_LENGTH = 128;
    struct DynamicLogEntry {
        char message[MAX_LINE_LENGTH];
        unsigned long firstTimestamp;
        unsigned long lastTimestamp;
        size_t count;
    };
    DynamicLogEntry* _dynamicLogBuffer;
#else
    static const size_t MAX_LINE_LENGTH = 48; // Reduce for Nano
    static const size_t MAX_LOG_ENTRIES = 3;
    char _logBuffer[MAX_LOG_ENTRIES][MAX_LINE_LENGTH]; // Static bound for Nano
#endif
    size_t _head;
    size_t _count;
    
    char _currentLine[MAX_LINE_LENGTH];
    size_t _currentLineLen;
    unsigned long _lastPrintTime;

    uint8_t _streamTargetId;
    unsigned long _streamTimeoutTime;
    LogStreamCallback _streamCallback;
    void* _streamCallbackContext;    
    
    const unsigned long PRINT_TIMEOUT_MS = 100;
    
    void flushCurrentLine();
    void addLogEntry(const char* entry);
};