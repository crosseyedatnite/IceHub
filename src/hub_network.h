#ifndef HUB_NETWORK_H
#define HUB_NETWORK_H

#include <Arduino.h>

#ifdef DEVICE_ROLE_HUB
#include <functional>

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "ice_protocol.h"

class HubNetwork {
public:
    HubNetwork();
    void begin();
    void loop();
    
    // Callback type for when a valid command is received via MQTT
    typedef std::function<void(const IcePacket&)> PacketCallback;
    void onPacketReceived(PacketCallback callback);

    // Proxy Methods
    void publishDiscovery(uint8_t nodeId, uint8_t deviceType);
    void publishState(uint8_t nodeId, const IcePacket& packet);
    
    // Input from Radio (Main loop passes packets here)
    void handleRadioPacket(const IcePacket& packet);
    
    // Output to Radio
    typedef std::function<void(uint8_t targetId, const IcePacket& packet)> RadioSendCallback;
    void onRadioSend(RadioSendCallback callback);

private:
    void setupWiFi();
    void setupWebServer();
    void reconnect();
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    
    // Config & NVS
    void loadConfig();
    String getNodeName(uint8_t nodeId);
    void saveNodeName(uint8_t nodeId, String name);
    void handleWebConfig(); // Process form submissions

    // Dynamic WiFi Methods
    void performSerialSetup();
    bool connectWithTimeout();

    // Multipart Reassembly
    void handleMultipartPacket(const IcePacket& packet);
    void processMultipartMessage(uint8_t nodeId, uint16_t type, uint8_t* data, size_t length);

    WiFiClient _espClient;
    PubSubClient _mqttClient;
    WebServer _webServer;
    PacketCallback _packetCallback;
    unsigned long _lastReconnectAttempt;
    RadioSendCallback _radioSendCallback;
    
    Preferences _preferences;
    String _ssid;
    String _password;
    
    // MQTT Config
    String _mqttServer;
    String _mqttUser;
    String _mqttPass;
    
    bool _seenNodes[256]; // Track nodes seen since boot for the UI
    bool _sessionSeenNodes[256]; // Track nodes actively seen in this execution context

    // Multipart Reassembly State
    uint8_t* _mpBuffer;
    size_t _mpExpectedSize;
    size_t _mpReceivedSize;
    uint8_t _mpSenderId;
    uint16_t _mpType;
    unsigned long _mpLastChunkTime;
};

#endif // DEVICE_ROLE_HUB
#endif // HUB_NETWORK_H