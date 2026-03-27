#include "RadarPresenceSensor.h"
#include <stdio.h>

RadarPresenceSensor::RadarPresenceSensor(IceHubLog& logger, uint8_t rxPin, uint8_t txPin, uint8_t triggerPin)
    : _logger(logger), 
      _radarSerial(rxPin, txPin), 
      _triggerPin(triggerPin), 
      _lastReadState(false),
      _debouncedState(false),
      _lastDebounceTime(0),
      _eventCallback(nullptr), 
      _eventContext(nullptr) {
}

void RadarPresenceSensor::setup() {
    _logger.println(F("RadarSensor: Initializing HLK-LD2420..."));
    
    pinMode(_triggerPin, INPUT);
    _radarSerial.begin(115200);
    
    _lastReadState = (digitalRead(_triggerPin) == HIGH);
    _debouncedState = _lastReadState;
    _lastDebounceTime = millis();
    _logger.println(F("RadarSensor: Initialized."));
}

void RadarPresenceSensor::loop() {
    // Clear any incoming serial data from the sensor to prevent buffer overflows
    while (_radarSerial.available() > 0) {
        _radarSerial.read();
    }

    bool reading = (digitalRead(_triggerPin) == HIGH);
    
    if (reading != _lastReadState) {
        _lastDebounceTime = millis();
        _lastReadState = reading;
    }

    if ((millis() - _lastDebounceTime) > 50) { // 50ms debounce
        if (reading != _debouncedState) {
            _debouncedState = reading;
            
            if (_debouncedState) {
                _logger.println(F("RadarSensor: Motion DETECTED."));
            } else {
                _logger.println(F("RadarSensor: Motion CLEARED."));
            }
            
            if (_eventCallback) {
                _eventCallback(_debouncedState, _eventContext);
            }
        }
    }
}

void RadarPresenceSensor::getCapabilitiesJSON(char* buffer, size_t maxLen) const {
    snprintf_P(buffer, maxLen, PSTR("{\"type\":\"binary_sensor\",\"capabilities\":[\"motion\"]}"));
}

void RadarPresenceSensor::onPresenceEvent(PresenceEventCallback callback, void* context) {
    _eventCallback = callback;
    _eventContext = context;
}