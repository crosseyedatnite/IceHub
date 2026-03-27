#include "ice_hub.h"
#ifdef DEVICE_ROLE_HUB
#include <nvs_flash.h>
#include <time.h>
#include <ESPmDNS.h>

extern void updateNodeSensorData(uint8_t id, float temp, float hum);
extern void publishNodeSensorDataHA(uint8_t id, float temp, float hum);
extern void appendRemoteLog(uint8_t id, const String& logLine);

#include "IceHubLog.h"
extern IceHubLog iceLog;

IceHub::IceHub(SystemConfig& config, TransportService& transport, NodeRegistry& registry, CapabilityManager& caps, WebAdapter& web, EffectController& effects)
    : _config(config), _transport(transport), _registry(registry), _caps(caps), _web(web), _effects(effects), 
      _awaitingResetConfirmation(false), _resetRequestTime(0),
      _discoveryIndex(1), _lastDiscoveryTime(0) {}

void IceHub::begin() {
    iceLog.println("IceHub Kernel Starting...");
    
    // 1. Initialize Services
    _registry.begin();
    _caps.begin();
    
    // Ensure hub_config exists
    _config.begin();
    
    // 2. Setup Infrastructure
    setupWiFi();
    _web.begin();
    
    // 3. Setup Transport Ingress (Event Routing)
    _transport.onMessageReceived([](const TransportService::Message& msg, void* ctx) {
        static_cast<IceHub*>(ctx)->handleMessage(msg);
    }, this);

    // 4. Setup Smart Broadcast Filter
    _transport.setBroadcastFilter([](uint8_t nodeId, uint16_t msgType, void* ctx) -> bool {
        return static_cast<IceHub*>(ctx)->_caps.isNodeInterested(nodeId, msgType);
    }, this);
    
    // 5. Start Radio (Hub is Node 0)
    _transport.begin(0);
}

void IceHub::loop() {
    _web.loop();
    checkSerial();

    // Background Discovery Agent
    // Scans known nodes to ensure we have their capabilities loaded
    if (millis() - _lastDiscoveryTime > 200) { // 5Hz scan rate
        _lastDiscoveryTime = millis();
        
        if (_registry.isNodeConfigured(_discoveryIndex) && !_caps.hasCapabilities(_discoveryIndex)) {
             iceLog.printf("Hub: Proactive Discovery -> Requesting Caps for Node %d\n", _discoveryIndex);
             ConfigPayload cfg;
             cfg.command = 1; // Get Caps
             _transport.sendMessage(_discoveryIndex, PACKET_CONFIG, &cfg, sizeof(cfg));
        }
        
        _discoveryIndex++;
        if (_discoveryIndex >= 255) _discoveryIndex = 1;
    }
}

void IceHub::setupWiFi() {
    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
        iceLog.printf("NVS Storage: %d used / %d free entries (Total: %d)\n", nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
    }

    _ssid = _config.getWifiSsid();
    _password = _config.getWifiPassword();
    _hostname = _config.getHostname();
    if (_hostname.length() == 0) {
        _hostname = "icehub";
        _config.setHostname(_hostname);
    }

    if (_ssid.length() > 0) {
        iceLog.println("Found stored WiFi credentials.");
        if (connectWithTimeout()) {
            return; // Connected successfully
        }
        iceLog.println("Stored credentials failed. Clearing storage...");
        _config.clearWifiConfig();
    }
    
    // If we are here, we either had no creds or they failed
    performSerialSetup();
}

bool IceHub::connectWithTimeout() {
    iceLog.printf("Connecting to %s\n", _ssid.c_str());
    
    WiFi.setHostname(_hostname.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        iceLog.print(".");
        if (millis() - startAttemptTime > 10000) { // 10 second timeout
            iceLog.println("\nConnection timed out.");
            return false;
        }
    }

    iceLog.println("");
    iceLog.println("WiFi connected");
    iceLog.print("IP address: ");
    iceLog.println(WiFi.localIP().toString().c_str());

    if (!MDNS.begin(_hostname.c_str())) {
        iceLog.println("Error setting up MDNS responder!");
    } else {
        iceLog.printf("mDNS responder started: http://%s.local\n", _hostname.c_str());
        MDNS.addService("http", "tcp", 80);
    }

    iceLog.println("Configuring NTP time sync...");
    String tz = _config.getTimezone();
    configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");

    return true;
}

void IceHub::performSerialSetup() {
    iceLog.println("\n\n--- WiFi Setup ---");
    iceLog.println("Scanning for networks...");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    
    if (n <= 0) {
        iceLog.println("No networks found.");
    } else {
        iceLog.printf("%d networks found:\n", n);
        for (int i = 0; i < n; ++i) {
            iceLog.printf("%d: %s (%d) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            delay(10);
        }
    }
    
    iceLog.println("\nEnter SSID (name or number from list):");
    while (Serial.available() == 0) { delay(100); }
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    int selection = input.toInt();
    if (selection > 0 && selection <= n) {
        _ssid = WiFi.SSID(selection - 1);
    } else {
        _ssid = input;
    }
    
    iceLog.printf("Selected SSID: %s\n", _ssid.c_str());
    
    iceLog.println("Enter Password:");
    while (Serial.available() == 0) { delay(100); }
    _password = Serial.readStringUntil('\n');
    _password.trim();
    
    iceLog.println("Enter Hostname (default: icehub):");
    while (Serial.available() == 0) { delay(100); }
    _hostname = Serial.readStringUntil('\n');
    _hostname.trim();
    if (_hostname.length() == 0) {
        _hostname = "icehub";
    }

    iceLog.println("Saving credentials...");
    _config.setWifiSsid(_ssid);
    _config.setWifiPassword(_password);
    _config.setHostname(_hostname);
    
    if (!connectWithTimeout()) {
        iceLog.println("Failed to connect. Retrying setup...");
        _config.clearWifiConfig();
        performSerialSetup(); // Recursive retry until success
    }
}

void IceHub::checkSerial() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) return;

        if (_awaitingResetConfirmation) {
            if (input.equalsIgnoreCase("y") || input.equalsIgnoreCase("yes")) {
                iceLog.println("Factory Resetting...");
                nvs_flash_erase();
                nvs_flash_init();
                ESP.restart();
            } else {
                iceLog.println("Reset cancelled.");
                _awaitingResetConfirmation = false;
            }
        } else {
            if (input.equalsIgnoreCase("factory_reset")) {
                iceLog.println("WARNING: Factory Reset requested.");
                iceLog.println("This will erase all WiFi and Node data.");
                iceLog.println("Type 'y' to confirm within 10 seconds.");
                _awaitingResetConfirmation = true;
                _resetRequestTime = millis();
            }
        }
    }
    
    if (_awaitingResetConfirmation && millis() - _resetRequestTime > 10000) {
         iceLog.println("Reset confirmation timed out.");
         _awaitingResetConfirmation = false;
    }
}

void IceHub::handleMessage(const TransportService::Message& msg) {
    // 1. Registration Request
    if (msg.type == PACKET_REGISTER_REQ) {
        // We need to access the payload. Since we don't have the full packet struct here,
        // we cast the data pointer to the specific payload struct defined in ice_protocol.h
        const RegistrationPayload* reg = (const RegistrationPayload*)msg.data;
        iceLog.printf("Hub: Received Registration Request from Node [Unassigned]. Device Type: %d\n", reg->deviceType);
            
        uint8_t newId = _registry.allocateNextId();
        uint32_t newToken = esp_random(); // Generate 32-bit secure token
        _registry.setNodeToken(newId, newToken);
        
        // Construct ACK Payload
        RegistrationPayload ackPayload;
        ackPayload.nonce = reg->nonce;
        ackPayload.assignedId = newId;
        ackPayload.token = newToken;
        
        // Send the ACK directly to the Node's Ephemeral ID Private Channel
        _transport.sendMessage(msg.sourceId, PACKET_REGISTER_ACK, &ackPayload, sizeof(ackPayload));
        
        _registry.markNodeSeen(newId);
        iceLog.printf("Hub: Ok, adding to Node Registry. Assigned ID: %d\n", newId);
        iceLog.printf("Hub: Fire 'Node %d Registered' event\n", newId);
    }
    
    // 2. Ping / Hello
    else if (msg.type == PACKET_PING) {
        if (msg.length >= sizeof(PingPayload)) {
            const PingPayload* ping = (const PingPayload*)msg.data;
            if (ping->token == _registry.getNodeToken(msg.sourceId)) {
                _registry.markNodeSeen(msg.sourceId);
                
                // If we don't have caps, ask for them
                if (_caps.getCapabilities(msg.sourceId).length() == 0) {
                     ConfigPayload cfg;
                     cfg.command = 1; // Get Caps
                     _transport.sendMessage(msg.sourceId, PACKET_CONFIG, &cfg, sizeof(cfg));
                }
            } else {
                iceLog.printf("Hub: Token mismatch for Node %d. Sending Invalidate command.\n", msg.sourceId);
                _transport.sendMessage(msg.sourceId, PACKET_INVALIDATE, nullptr, 0);
            }
        }
    }
    
    // 3. Capabilities Response (Multipart Type 1)
    else if (msg.type == 1 && msg.length > 0) {
        iceLog.printf("Hub: Transport received Capabilities Response for Node %d. Dispatching.\n", msg.sourceId);
        _caps.processCapabilities(msg.sourceId, (const char*)msg.data);
    }
    
    // 4. Remote Log Stream (Multipart Type 2)
    else if (msg.type == 2 && msg.length > 0) {
        char safeBuf[256];
        size_t safeLen = msg.length < 255 ? msg.length : 255;
        memcpy(safeBuf, msg.data, safeLen);
        safeBuf[safeLen] = '\0';
        appendRemoteLog(msg.sourceId, String(safeBuf));
    }
    
    // 5. Sensor Data
    else if (msg.type == PACKET_SENSOR) {
        if (msg.length >= sizeof(SensorPayload)) {
            const SensorPayload* payload = (const SensorPayload*)msg.data;
            float realTemp = payload->temperature / 100.0f;
            float realHum = payload->humidity / 100.0f;
            iceLog.printf("Hub: Received Sensor Data from Node %d: Temp=%.1fC, Hum=%.1f%%\n", msg.sourceId, realTemp, realHum);
            updateNodeSensorData(msg.sourceId, realTemp, realHum);
            publishNodeSensorDataHA(msg.sourceId, realTemp, realHum);
        }
    }
}
#endif