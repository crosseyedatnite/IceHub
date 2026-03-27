#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB
#include <Preferences.h>

class SystemConfig {
public:
    SystemConfig();
    
    void begin(); // Run any initial setup if needed

    // WiFi Settings
    String getHostname();
    void setHostname(const String& hostname);
    
    String getWifiSsid();
    void setWifiSsid(const String& ssid);
    
    String getWifiPassword();
    void setWifiPassword(const String& password);
    
    void clearWifiConfig();

    // MQTT & System Settings
    String getMqttServer();
    void setMqttServer(const String& server);
    String getMqttUser();
    void setMqttUser(const String& user);
    String getMqttPassword();
    void setMqttPassword(const String& password);
    
    String getTimezone();
    void setTimezone(const String& tz);
};
#endif // DEVICE_ROLE_HUB
#endif