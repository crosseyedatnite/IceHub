#include "hub_network.h"

#ifdef DEVICE_ROLE_HUB
#include <ArduinoJson.h>
#include "ice_effects.h" // Needed for effect list strings
#include <nvs_flash.h>

HubNetwork::HubNetwork() : _mqttClient(_espClient), _lastReconnectAttempt(0), 
    _mpBuffer(nullptr), _mpExpectedSize(0), _mpReceivedSize(0), _mpSenderId(0), _mpType(0), _mpLastChunkTime(0) {
    memset(_seenNodes, 0, sizeof(_seenNodes));
    memset(_sessionSeenNodes, 0, sizeof(_sessionSeenNodes));
}

void HubNetwork::begin() {
    loadConfig();
    setupWiFi();
    setupWebServer();
    
    if (_mqttServer.length() > 0) {
        Serial.println("Configuring MQTT...");
        _mqttClient.setServer(_mqttServer.c_str(), 1883);
        _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
    } else {
        Serial.println("No MQTT server configured. Please set it up in the web interface.");
    }
}

void HubNetwork::loadConfig() {
    _preferences.begin("hub_config", false); // Read-Write to create if missing
    _mqttServer = _preferences.getString("mqtt_server", "");
    _mqttUser   = _preferences.getString("mqtt_user", "");
    _mqttPass   = _preferences.getString("mqtt_pass", "");
    Serial.println("Loaded MQTT Config:");
    Serial.println("MQTT Server: " + _mqttServer);
    Serial.println("MQTT User: " + _mqttUser);
    _preferences.end();

    // Pre-load known nodes into seen list so they appear in UI immediately
    _preferences.begin("hub_nodes", true);
    for(int i=1; i<255; i++) {
        if(_preferences.getString(String(i).c_str(), "").length() > 0) {
            _seenNodes[i] = true;
        }
    }
    _preferences.end();
}

void HubNetwork::setupWiFi() {
    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
        Serial.printf("NVS Storage: %d used / %d free entries (Total: %d)\n", nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
    }

    _preferences.begin("wifi_config", false); // Open NVS in Read/Write mode
    
    _ssid = _preferences.getString("ssid", "");
    _password = _preferences.getString("password", "");

    if (_ssid.length() > 0) {
        Serial.println("Found stored WiFi credentials.");
        if (connectWithTimeout()) {
            _preferences.end();
            return; // Connected successfully
        }
        Serial.println("Stored credentials failed. Clearing storage...");
        _preferences.clear();
    }
    
    // If we are here, we either had no creds or they failed
    performSerialSetup();
    _preferences.end();
}

bool HubNetwork::connectWithTimeout() {
    Serial.print("Connecting to ");
    Serial.println(_ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - startAttemptTime > 10000) { // 10 second timeout
            Serial.println("\nConnection timed out.");
            return false;
        }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
}

void HubNetwork::performSerialSetup() {
    Serial.println("\n\n--- WiFi Setup ---");
    Serial.println("Scanning for networks...");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    
    if (n <= 0) {
        Serial.println("No networks found.");
    } else {
        Serial.print(n);
        Serial.println(" networks found:");
        for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (%d) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            delay(10);
        }
    }
    
    Serial.println("\nEnter SSID (name or number from list):");
    while (Serial.available() == 0) { delay(100); }
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    int selection = input.toInt();
    if (selection > 0 && selection <= n) {
        _ssid = WiFi.SSID(selection - 1);
    } else {
        _ssid = input;
    }
    
    Serial.print("Selected SSID: "); Serial.println(_ssid);
    
    Serial.println("Enter Password:");
    while (Serial.available() == 0) { delay(100); }
    _password = Serial.readStringUntil('\n');
    _password.trim();
    
    Serial.println("Saving credentials...");
    _preferences.putString("ssid", _ssid);
    _preferences.putString("password", _password);
    
    if (!connectWithTimeout()) {
        Serial.println("Failed to connect. Retrying setup...");
        _preferences.clear();
        performSerialSetup(); // Recursive retry until success
    }
}

void HubNetwork::setupWebServer() {
    _webServer.on("/", [this]() {
        Serial.println("Serving configuration page...");
        String html = "<html><head><style>body{font-family:sans-serif;padding:20px;} .node{border:1px solid #ccc;padding:10px;margin:5px 0;}</style></head><body>";
        html += "<h1>IceHub Gateway</h1>";
        html += "<p><b>Status:</b> " + String(_mqttClient.connected() ? "<span style='color:green'>MQTT Connected</span>" : "<span style='color:red'>MQTT Disconnected</span>") + "</p>";
        
        html += "<h2>Configuration</h2>";
        html += "<form action='/config' method='POST'>";
        html += "MQTT Server: <input type='text' name='server' value='" + _mqttServer + "'><br>";
        html += "MQTT User: <input type='text' name='user' value='" + _mqttUser + "'><br>";
        html += "MQTT Pass: <input type='password' name='pass' value='" + _mqttPass + "'><br>";
        html += "<input type='submit' value='Save & Reboot'>";
        html += "</form>";

        html += "<h2>Detected Nodes</h2>";
        for(int i=1; i<255; i++) {
            if(_seenNodes[i]) {
                String name = getNodeName(i);
                html += "<div class='node'>";
                html += "<b>Node ID " + String(i) + "</b> ";
                html += "<form action='/name' method='POST' style='display:inline'>";
                html += "<input type='hidden' name='id' value='" + String(i) + "'>";
                html += "Name: <input type='text' name='name' value='" + name + "'>";
                html += "<input type='submit' value='Set Name'>";
                html += "</form>";
                if(name.length() == 0) html += " <span style='color:orange'>(Unconfigured)</span>";
                else {
                    html += " <span style='color:green'>(Active)</span>";
                    
                    // --- Render Dynamic Control Panel ---
                    _preferences.begin("hub_caps", true);
                    String capsJson = _preferences.getString(String(i).c_str(), "");
                    _preferences.end();

                    if (capsJson.length() > 0) {
                        StaticJsonDocument<1024> doc;
                        deserializeJson(doc, capsJson);
                        
                        html += "<div style='margin-top:5px;background:#f0f0f0;padding:5px;'>";
                        html += "LEDs: " + doc["leds"].as<String>() + " | Mode: <b>" + doc["mode"].as<String>() + "</b><br>";
                        
                        // Brightness Slider (Simple form submission)
                        // Note: In a real app, this would use AJAX/Fetch to avoid page reloads
                        html += "Brightness: " + doc["bri"].as<String>();
                        
                        // Mode Buttons
                        html += "<br>";
                        JsonArray modes = doc["modes"];
                        for(JsonVariant v : modes) {
                            String m = v.as<String>();
                            // Using a simple link for now to trigger MQTT command logic (simulated)
                            // In reality, this should post to a handler that sends a radio packet
                            html += "<button onclick=\"fetch('/cmd?id=" + String(i) + "&mode=" + m + "')\">" + m + "</button> ";
                        }
                        html += "</div>";
                    }
                }
                html += "</div>";
            }
        }
        html += "</body></html>";
        _webServer.send(200, "text/html", html);
    });

    _webServer.on("/config", HTTP_POST, [this]() {
        if(_webServer.hasArg("server")) {
            _preferences.begin("hub_config", false);
            _preferences.putString("mqtt_server", _webServer.arg("server"));
            _preferences.putString("mqtt_user", _webServer.arg("user"));
            _preferences.putString("mqtt_pass", _webServer.arg("pass"));
            _preferences.end();
            _webServer.send(200, "text/html", "Saved. Rebooting...<meta http-equiv='refresh' content='3;url=/'>");
            Serial.println("Configuration updated via web interface. Rebooting...");
            Serial.println("New MQTT Server: " + _webServer.arg("server"));
            Serial.println("New MQTT User: " + _webServer.arg("user"));
            delay(1000);
            ESP.restart();
        }
    });

    _webServer.on("/name", HTTP_POST, [this]() {
        if(_webServer.hasArg("id") && _webServer.hasArg("name")) {
            uint8_t id = _webServer.arg("id").toInt();
            String name = _webServer.arg("name");
            saveNodeName(id, name);
            // Trigger discovery immediately after naming
            publishDiscovery(id, 1); // Assuming Type 1 (LED) for now
            _webServer.send(200, "text/html", "Name Saved. <a href='/'>Back</a>");
        }
    });

    // Simple Command Handler for Web UI Buttons
    _webServer.on("/cmd", HTTP_GET, [this]() {
        if(_webServer.hasArg("id") && _webServer.hasArg("mode")) {
            uint8_t id = _webServer.arg("id").toInt();
            String mode = _webServer.arg("mode");
            
            // Map string mode to ID (Simple lookup or pass string if supported)
            // For now, we just acknowledge. 
            // TODO: Implement HubNetwork::sendEffectCommand(id, modeString)
            Serial.printf("Web Command: Node %d -> Mode %s\n", id, mode.c_str());
            
            _webServer.send(200, "text/plain", "OK");
        }
    });
    
    _webServer.begin();
    Serial.println("Web server started");
}

String HubNetwork::getNodeName(uint8_t nodeId) {
    _preferences.begin("hub_nodes", true);
    String name = _preferences.getString(String(nodeId).c_str(), "");
    _preferences.end();
    return name;
}

void HubNetwork::saveNodeName(uint8_t nodeId, String name) {
    _preferences.begin("hub_nodes", false);
    _preferences.putString(String(nodeId).c_str(), name);
    _preferences.end();
}

void HubNetwork::loop() {
    if (_mqttServer.length() > 0 && !_mqttClient.connected()) {
        reconnect();
    }
    _mqttClient.loop();
    _webServer.handleClient();

    // Multipart Timeout (2 seconds)
    if (_mpBuffer && (millis() - _mpLastChunkTime > 2000)) {
        Serial.println("Multipart transfer timed out. Discarding buffer.");
        free(_mpBuffer);
        _mpBuffer = nullptr;
        _mpExpectedSize = 0;
        _mpReceivedSize = 0;
    }
}

void HubNetwork::handleRadioPacket(const IcePacket& packet) {
    Serial.printf("Received packet from Node %d, Type %d\n", packet.senderID, packet.msgType);
    
    bool isFirstSessionPacket = false;

    // Mark node as seen for the UI
    if(packet.senderID > 0) {
        _seenNodes[packet.senderID] = true;
        
        if (!_sessionSeenNodes[packet.senderID]) {
            _sessionSeenNodes[packet.senderID] = true;
            isFirstSessionPacket = true;
        }

        // If we have a name, ensure we are connected/discovered
        // (Real logic would check if discovery was already sent this session)
        String name = getNodeName(packet.senderID);
        if(name.length() > 0) {
            // Node is configured, we can process data or ensure discovery
            // For now, we rely on the Web UI 'Set Name' to trigger the initial discovery
        }
    }

    if (packet.msgType == PACKET_MULTIPART) {
        handleMultipartPacket(packet);
        return;
    }

    // Handle "Hello" from already registered nodes booting up
    if (packet.msgType == PACKET_PING) {
        if (isFirstSessionPacket && _radioSendCallback) {
            Serial.printf("Node %d is online. Requesting Capabilities...\n", packet.senderID);
            IcePacket reqCaps;
            memset(&reqCaps, 0, sizeof(reqCaps));
            reqCaps.senderID = 0;
            reqCaps.targetID = packet.senderID;
            reqCaps.msgType = PACKET_CONFIG;
            reqCaps.payload.config.command = 1; // 1 = Get Capabilities
            _radioSendCallback(packet.senderID, reqCaps);
        }
    }

    // Handle Registration Requests
    if (packet.msgType == PACKET_REGISTER_REQ) {
        if (_radioSendCallback) {
            _preferences.begin("hub_config", false);
            uint8_t nextId = _preferences.getUChar("next_id", 1);
            
            // Increment for next time (wrap 1-254)
            uint8_t saveId = nextId + 1;
            if (saveId >= 255) saveId = 1;
            _preferences.putUChar("next_id", saveId);
            _preferences.end();
            
            IcePacket ack;
            ack.senderID = 0;
            ack.targetID = 255; // Broadcast/Unassigned channel
            ack.msgType = PACKET_REGISTER_ACK;
            ack.payload.registration.nonce = packet.payload.registration.nonce;
            ack.payload.registration.assignedId = nextId;
            
            _radioSendCallback(255, ack);
            
            // Mark the new ID as seen so it appears in UI immediately
            _seenNodes[nextId] = true;
            Serial.print("Registered New Node. Assigned ID: ");
            Serial.println(nextId);

            // --- Step 2: Request Capabilities ---
            delay(50); // Give node time to process ACK
            IcePacket reqCaps;
            reqCaps.senderID = 0;
            reqCaps.targetID = nextId;
            reqCaps.msgType = PACKET_CONFIG;
            reqCaps.payload.config.command = 1; // 1 = Get Capabilities
            _radioSendCallback(nextId, reqCaps);
        }
    }
}

void HubNetwork::handleMultipartPacket(const IcePacket& packet) {
    uint16_t segmentId = packet.payload.multipart.segmentId;

    if (segmentId == 0) {
        // --- Header Packet ---
        uint16_t totalSize = packet.payload.multipart.header.totalSize;
        uint16_t type = packet.payload.multipart.header.type;
        
        // Reset any existing transfer
        if (_mpBuffer) {
            free(_mpBuffer);
            _mpBuffer = nullptr;
        }
        
        // Safety limit (e.g., 4KB) to prevent memory exhaustion
        if (totalSize > 4096) {
            Serial.println("Multipart message too large, rejecting.");
            return;
        }

        _mpBuffer = (uint8_t*)malloc(totalSize + 1); // +1 for null terminator
        if (!_mpBuffer) {
            Serial.println("Multipart memory allocation failed.");
            return;
        }
        
        _mpExpectedSize = totalSize;
        _mpReceivedSize = 0;
        _mpSenderId = packet.senderID;
        _mpType = type;
        _mpLastChunkTime = millis();
        
        Serial.printf("Starting multipart receive from Node %d. Size: %d, Type: %d\n", packet.senderID, totalSize, type);
        
    } else {
        // --- Data Chunk ---
        if (!_mpBuffer || packet.senderID != _mpSenderId) {
            return; // Ignore unexpected chunks or interleaved senders
        }
        
        size_t offset = (segmentId - 1) * 27;
        if (offset >= _mpExpectedSize) return; // Out of bounds
        
        size_t chunkSize = _mpExpectedSize - offset;
        if (chunkSize > 27) chunkSize = 27;
        
        memcpy(_mpBuffer + offset, packet.payload.multipart.data, chunkSize);
        _mpReceivedSize += chunkSize;
        _mpLastChunkTime = millis();
        
        if (_mpReceivedSize >= _mpExpectedSize) {
            Serial.println("Multipart transfer complete.");
            _mpBuffer[_mpExpectedSize] = 0; // Null terminate string
            
            processMultipartMessage(_mpSenderId, _mpType, _mpBuffer, _mpExpectedSize);
            
            free(_mpBuffer);
            _mpBuffer = nullptr;
            _mpExpectedSize = 0;
            _mpReceivedSize = 0;
        }
    }
}

void HubNetwork::processMultipartMessage(uint8_t nodeId, uint16_t type, uint8_t* data, size_t length) {
    if (type == 1) { // JSON Capabilities
        Serial.printf("Received Capabilities from Node %d (%d bytes):\n", nodeId, length);
        Serial.println((char*)data);

        // Store in NVS for Web UI
        _preferences.begin("hub_caps", false);
        _preferences.putString(String(nodeId).c_str(), (char*)data);
        _preferences.end();
    }
}

void HubNetwork::onPacketReceived(PacketCallback callback) {
    _packetCallback = callback;
}

void HubNetwork::onRadioSend(RadioSendCallback callback) {
    _radioSendCallback = callback;
}

void HubNetwork::reconnect() {
    unsigned long now = millis();
    if (now - _lastReconnectAttempt > 5000) {
        _lastReconnectAttempt = now;
        Serial.print("Attempting MQTT connection...");
        
        bool connected = false;
        if(_mqttUser.length() > 0) {
            connected = _mqttClient.connect("IceHubGateway", _mqttUser.c_str(), _mqttPass.c_str());
        } else {
            connected = _mqttClient.connect("IceHubGateway");
        }

        if (connected) {
            Serial.println("connected");
            _mqttClient.subscribe("home/lights/+/set");
        } else {
            Serial.print("failed, rc=");
            Serial.print(_mqttClient.state());
            Serial.println(" try again in 5 seconds");
        }
    }
}

void HubNetwork::mqttCallback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("JSON Error: ");
        Serial.println(error.c_str());
        return;
    }

    // Topic format: home/lights/[FRIENDLY_NAME]/set
    String topicStr = String(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    int secondLastSlash = topicStr.lastIndexOf('/', lastSlash - 1);
    String targetName = topicStr.substring(secondLastSlash + 1, lastSlash);
    
    // Reverse lookup: Find ID for Name
    // This is inefficient (O(N) scan of NVS), but fine for < 20 nodes.
    uint8_t targetNodeId = 0;
    _preferences.begin("hub_nodes", true);
    // Simple scan 1-50 for now
    for(int i=1; i<50; i++) {
        if(_preferences.getString(String(i).c_str(), "") == targetName) {
            targetNodeId = i;
            break;
        }
    }
    _preferences.end();

    if (targetNodeId == 0) {
        Serial.println("Unknown Node Name in topic");
        return;
    }

    IcePacket packet;
    packet.msgType = PACKET_STATE;
    packet.targetID = targetNodeId;
    packet.senderID = 0; // Hub

    // Default values (255 = ignore/no change)
    packet.payload.effect.mode = 255;
    packet.payload.effect.brightness = 255;

    if (doc.containsKey("state")) {
        if (strcmp(doc["state"], "OFF") == 0) {
            packet.payload.effect.mode = 0; // OFF
        }
    }

    if (doc.containsKey("brightness")) {
        packet.payload.effect.brightness = doc["brightness"];
    }

    if (doc.containsKey("effect")) {
        // const char* effectName = doc["effect"];
        // TODO: Map string name to ID using IceEffects helper
    }

    if (doc.containsKey("color")) {
        packet.payload.effect.mode = 1; // STATIC_COLOR
        packet.payload.effect.r = doc["color"]["r"];
        packet.payload.effect.g = doc["color"]["g"];
        packet.payload.effect.b = doc["color"]["b"];
    }

    if (_packetCallback) {
        _packetCallback(packet);
    }
}

void HubNetwork::publishDiscovery(uint8_t nodeId, uint8_t deviceType) {
    if (!_mqttClient.connected()) return;

    String friendlyName = getNodeName(nodeId);
    if(friendlyName.length() == 0) return; // Don't discover unnamed nodes

    // Sanitize name for ID (lowercase, no spaces)
    String safeName = friendlyName;
    safeName.toLowerCase();
    safeName.replace(" ", "_");

    String discoveryTopic = String("homeassistant/light/") + safeName + "/config";
    
    StaticJsonDocument<512> doc;
    doc["name"] = friendlyName;
    doc["unique_id"] = "ice_node_" + String(nodeId);
    doc["state_topic"] = String("home/lights/") + safeName + "/state";
    doc["command_topic"] = String("home/lights/") + safeName + "/set";
    doc["schema"] = "json";
    doc["brightness"] = true;
    doc["effect"] = true;
    
    // We can use the shared IceEffects header to get the list string
    // doc["effect_list"] = serialized(IceEffects::getEffectList()); 
    // Hardcoding for now to avoid dependency issues if IceEffects is reverted
    doc["effect_list"] = serialized("[\"RAINBOW\",\"TRAIL\",\"WAVE\",\"CONFETTI\",\"JUGGLE\",\"OFF\"]");

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "ice_node_" + String(nodeId);
    device["name"] = friendlyName;
    device["model"] = (deviceType == 1) ? "RF-Nano LED" : "RF-Nano Sensor";
    device["sw_version"] = "1.0";

    char buffer[512];
    serializeJson(doc, buffer);
    _mqttClient.publish(discoveryTopic.c_str(), buffer, true);
    
    Serial.print("Sent Discovery for ");
    Serial.println(friendlyName);
}

void HubNetwork::publishState(uint8_t nodeId, const IcePacket& packet) {
    // TODO: Convert binary packet back to JSON state and publish to MQTT
    // topic: home/lights/ice_node_XX/state
}

#endif