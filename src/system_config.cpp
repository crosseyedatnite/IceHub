#include "system_config.h"
#ifdef DEVICE_ROLE_HUB

SystemConfig::SystemConfig() {}

void SystemConfig::begin() {
    // Ensure necessary namespaces exist in NVS
    Preferences prefs;
    prefs.begin("hub_config", false);
    prefs.end();
}

// --- WiFi Settings (Namespace: wifi_config) ---

String SystemConfig::getHostname() {
    Preferences prefs;
    prefs.begin("wifi_config", true);
    String val = prefs.getString("hostname", "icehub");
    prefs.end();
    return val;
}

void SystemConfig::setHostname(const String& hostname) {
    Preferences prefs;
    prefs.begin("wifi_config", false);
    prefs.putString("hostname", hostname);
    prefs.end();
}

String SystemConfig::getWifiSsid() {
    Preferences prefs;
    prefs.begin("wifi_config", true);
    String val = prefs.getString("ssid", "");
    prefs.end();
    return val;
}

void SystemConfig::setWifiSsid(const String& ssid) {
    Preferences prefs;
    prefs.begin("wifi_config", false);
    prefs.putString("ssid", ssid);
    prefs.end();
}

String SystemConfig::getWifiPassword() {
    Preferences prefs;
    prefs.begin("wifi_config", true);
    String val = prefs.getString("password", "");
    prefs.end();
    return val;
}

void SystemConfig::setWifiPassword(const String& password) {
    Preferences prefs;
    prefs.begin("wifi_config", false);
    prefs.putString("password", password);
    prefs.end();
}

void SystemConfig::clearWifiConfig() {
    Preferences prefs;
    prefs.begin("wifi_config", false);
    prefs.clear();
    prefs.end();
}

// --- MQTT & System Settings (Namespace: hub_config) ---

String SystemConfig::getTimezone() {
    Preferences prefs;
    prefs.begin("hub_config", true);
    String val = prefs.getString("timezone", "EST5EDT,M3.2.0,M11.1.0");
    prefs.end();
    return val;
}

void SystemConfig::setTimezone(const String& tz) {
    Preferences prefs;
    prefs.begin("hub_config", false);
    prefs.putString("timezone", tz);
    prefs.end();
}

String SystemConfig::getMqttServer() {
    Preferences prefs;
    prefs.begin("hub_config", true);
    String val = prefs.getString("mqtt_server", "");
    prefs.end();
    return val;
}

void SystemConfig::setMqttServer(const String& server) {
    Preferences prefs;
    prefs.begin("hub_config", false);
    prefs.putString("mqtt_server", server);
    prefs.end();
}

String SystemConfig::getMqttUser() {
    Preferences prefs;
    prefs.begin("hub_config", true);
    String val = prefs.getString("mqtt_user", "");
    prefs.end();
    return val;
}

void SystemConfig::setMqttUser(const String& user) {
    Preferences prefs;
    prefs.begin("hub_config", false);
    prefs.putString("mqtt_user", user);
    prefs.end();
}

String SystemConfig::getMqttPassword() {
    Preferences prefs;
    prefs.begin("hub_config", true);
    String val = prefs.getString("mqtt_pass", "");
    prefs.end();
    return val;
}

void SystemConfig::setMqttPassword(const String& password) {
    Preferences prefs;
    prefs.begin("hub_config", false);
    prefs.putString("mqtt_pass", password);
    prefs.end();
}

#endif