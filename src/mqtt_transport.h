#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "ice_service.h"

class MqttTransport : public IceService {
public:
    // Callback signature: topic, payload, length, context
    using MessageCallback = void (*)(const char*, const uint8_t*, size_t, void*);
    using ConnectCallback = void (*)(void* context);

    MqttTransport();
    void begin();
    void loop() override;

    bool publish(const char* topic, const char* payload, bool retained = false);
    void subscribe(const char* topic);
    void onMessage(MessageCallback callback, void* context);
    void onConnect(ConnectCallback callback, void* context);
    
    bool isConnected();

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    Preferences _prefs;
    
    String _server;
    uint16_t _port;
    String _user;
    String _pass;
    
    MessageCallback _callback;
    void* _context;
    ConnectCallback _connectCallback;
    void* _connectContext;
    unsigned long _lastReconnectAttempt;
    
    void loadConfig();
    void reconnect();
};

#endif // DEVICE_ROLE_HUB
#endif