### IceHub Next Additions

New connections added to hardware build for remote_nano.  They are:
- Si7021 Temperature/Humidity Sensor
- HLK-LD2420 mmWave Radar Presence Sensor

### Pinouts

| Component | Wire Color | Device Pin | Routing Path | Arduino Nano Pin |
| :--- | :--- | :--- | :--- | :--- |
| Si7021 | Red | VIN | Direct | 5V Bus |
| Si7021 | Black | GND | Direct | GND |
| Si7021 | Light Yellow | SCL | Direct | A5 (Hardware SCL) |
| Si7021 | Light Blue | SDA | Direct | A4 (Hardware SDA) |
| HLK-LD2420 | Red | 3.3V | Via AMS1117-3.3V | Isolated AMS1117-3.3V Regulator |
| HLK-LD2420 | Black | GND | Direct | GND |
| HLK-LD2420 | Green | OT1 (TX) | Via Level Converter (LV → HV) | D5 (Software RX) |
| HLK-LD2420 | Light Blue | RX | Via Level Converter (LV ← HV) | D3 (Software TX) |
| HLK-LD2420 | Light Yellow | OT2 (Trigger) | Direct | D4 (Digital Input) |

### Code Changes

- Implement TempHumiditySensor class 
    - Should be an IceService 
    - Should provide method getCapabilitiesJSON to allow it to be registered as a Sensor with the Hub and thus, as a sensor for HomeAssistant
    - Should have method to return currently measured Temperature and Humidity
    - On Temp and/or Humidity Changes within defined tolerance, it should intiate a callback to Hub with new value
    - Hub should expose it as a capability and reporting via MQTT

- Implement RadarPresenceSensor class
    - Should be an IceService 
    - Should provide method getCapabilitiesJSON to allow it to be registered as a Sensor with the Hub and thus as a boolean state sensor for HomeAssistant
    - Should have method to return currently measured Presence State
    - On Presenet State change it should initiate a callback to Hub with the new State
    - Hub should expose it as a capability and reporting via MQTT

- Implement IceHubLog class
    - Should be an IceService
    - Designed to allow web interface to be able to access latest log messages
    - Configured with how many of latest log entries to retain (FIFO)
    - Provide methods print and println with similar parameter list to Serial.print and Serial.println 
        - both methods also forward to Serial.print and println
        - print starts constructing a new log entry, println finalizes and logs it, with a timeout on print without println that after 100ms it will finalize and log it
    - Provide methods getCurrentLogCount() and getLogEntry(int offsetFromLatest)
    - Remotes can be sent a message requesting latest logs, they will respond with latest logs, probably handled at a higher level.  Need to refine this design.
    - This class should be injected as a dependency on all other classes that want to log to Serial. 

- Expand web interface to expose new sensors and be able to show logs from Hub or have Hub fetch logs from a node.