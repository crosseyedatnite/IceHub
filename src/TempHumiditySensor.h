#ifdef HAS_Si7021_SENSOR
#ifndef TEMP_HUMIDITY_SENSOR_H
#define TEMP_HUMIDITY_SENSOR_H

#include <Arduino.h>
#include "ice_service.h"
#include "IceHubLog.h"
#include <Adafruit_Si7021.h>

class TempHumiditySensor : public IceService {
public:
    typedef void (*SensorEventCallback)(float temp, float hum, void* context);

    TempHumiditySensor(IceHubLog& logger);
    
    void setup();
    void loop() override;
    
    void getCapabilitiesJSON(char* buffer, size_t maxLen) const;
    void onSensorEvent(SensorEventCallback callback, void* context);

private:
    IceHubLog& _logger;
    Adafruit_Si7021 _sensor;
    
    float _lastTemp;
    float _lastHum;
    unsigned long _lastReadTime;
    unsigned long _lastReportTime;
    
    SensorEventCallback _eventCallback;
    void* _eventContext;
};

#endif

#endif // HAS_Si7021_SENSOR