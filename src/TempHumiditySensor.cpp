#ifdef HAS_Si7021_SENSOR

#include "TempHumiditySensor.h"
#include <stdio.h>
#include <math.h>

#ifndef POLL_INTERVAL_MS
#define POLL_INTERVAL_MS 5000UL
#endif

#ifndef REPORT_INTERVAL_MS
#define REPORT_INTERVAL_MS 60000UL
#endif

#ifndef TEMP_TOLERANCE
#define TEMP_TOLERANCE 0.5f
#endif

#ifndef HUM_TOLERANCE
#define HUM_TOLERANCE 1.0f
#endif

TempHumiditySensor::TempHumiditySensor(IceHubLog& logger)
    : _logger(logger), 
      _lastTemp(-999.0f), 
      _lastHum(-999.0f), 
      _lastReadTime(0),
      _lastReportTime(0),
      _eventCallback(nullptr), 
      _eventContext(nullptr) {
}

void TempHumiditySensor::setup() {
    _logger.println(F("TempHumiditySensor: Initializing Si7021..."));
    
    if (!_sensor.begin()) {
        _logger.println(F("TempHumiditySensor: ERROR - Did not find Si7021 sensor! Check wiring."));
    } else {
        _logger.println(F("TempHumiditySensor: Si7021 Initialized successfully."));
    }
}

void TempHumiditySensor::loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - _lastReadTime >= POLL_INTERVAL_MS) {
        _lastReadTime = currentMillis;
        
        float currentTemp = _sensor.readTemperature();
        float currentHum = _sensor.readHumidity();
        
        if (isnan(currentTemp) || isnan(currentHum)) {
            _logger.println(F("TempHumiditySensor: Failed to read from sensor!"));
            return;
        }

        bool reportNeeded = false;
        if (fabs(currentTemp - _lastTemp) > TEMP_TOLERANCE) {
            reportNeeded = true;
        }
        if (fabs(currentHum - _lastHum) > HUM_TOLERANCE) {
            reportNeeded = true;
        }
        
        if (_lastReportTime == 0 || (currentMillis - _lastReportTime >= REPORT_INTERVAL_MS)) {
            reportNeeded = true;
        }
        
        if (reportNeeded) {
            _lastTemp = currentTemp;
            _lastHum = currentHum;
            _lastReportTime = currentMillis;
            
            // Extract integer and fractional parts for integer-only formatting
            int tempInt = (int)_lastTemp;
            int tempFrac = (int)(_lastTemp * 10.0f) % 10;
            if (tempFrac < 0) tempFrac = -tempFrac;
            
            int humInt = (int)_lastHum;
            int humFrac = (int)(_lastHum * 10.0f) % 10;
            if (humFrac < 0) humFrac = -humFrac;

            char logBuf[64];
            if (_lastTemp < 0 && tempInt == 0) {
                snprintf_P(logBuf, sizeof(logBuf), PSTR("TempHumiditySensor: Temp=-0.%dC, Hum=%d.%d%%"), tempFrac, humInt, humFrac);
            } else {
                snprintf_P(logBuf, sizeof(logBuf), PSTR("TempHumiditySensor: Temp=%d.%dC, Hum=%d.%d%%"), tempInt, tempFrac, humInt, humFrac);
            }
            _logger.println(logBuf);
            
            if (_eventCallback) {
                _eventCallback(_lastTemp, _lastHum, _eventContext);
            }
        }
    }
}

void TempHumiditySensor::getCapabilitiesJSON(char* buffer, size_t maxLen) const {
    snprintf_P(buffer, maxLen, PSTR("{\"type\":\"sensor\",\"capabilities\":[\"temperature\",\"humidity\"]}"));
}

void TempHumiditySensor::onSensorEvent(SensorEventCallback callback, void* context) {
    _eventCallback = callback;
    _eventContext = context;
}

#endif // HAS_Si7021_SENSOR