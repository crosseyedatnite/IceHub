#ifndef RADAR_PRESENCE_SENSOR_H
#define RADAR_PRESENCE_SENSOR_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "ice_service.h"
#include "IceHubLog.h"

class RadarPresenceSensor : public IceService {
public:
    typedef void (*PresenceEventCallback)(bool isPresent, void* context);

    RadarPresenceSensor(IceHubLog& logger, uint8_t rxPin = 5, uint8_t txPin = 3, uint8_t triggerPin = 4);
    
    void setup();
    void loop() override;
    
    void getCapabilitiesJSON(char* buffer, size_t maxLen) const;
    void onPresenceEvent(PresenceEventCallback callback, void* context);

private:
    IceHubLog& _logger;
    SoftwareSerial _radarSerial;
    uint8_t _triggerPin;
    
    bool _lastReadState;
    bool _debouncedState;
    unsigned long _lastDebounceTime;
    
    PresenceEventCallback _eventCallback;
    void* _eventContext;
};

#endif