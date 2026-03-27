#include "api_handler.h"
#ifdef DEVICE_ROLE_HUB

#include <ArduinoJson.h>
#include <uri/UriBraces.h>
#include <time.h>
#include "system_config.h"
#include <map>
#include <vector>

#include "IceHubLog.h"
extern IceHubLog iceLog;

extern TransportService transport;

// Sensor Caching state is tightly coupled to the API scope
struct SensorCache {
    float temperature = 0.0f;
    float humidity = 0.0f;
    unsigned long lastUpdate = 0;
    bool valid = false;
};
static SensorCache nodeSensorCache[256]; 

// Globally accessible function to update the cache (e.g. from Transport/Hub logic)
void updateNodeSensorData(uint8_t id, float temp, float hum) {
    nodeSensorCache[id].temperature = temp;
    nodeSensorCache[id].humidity = hum;
    nodeSensorCache[id].lastUpdate = millis();
    nodeSensorCache[id].valid = true;
}

// Remote Log Cache state
static std::map<uint8_t, std::vector<String>> remoteLogsCache;
static std::map<uint8_t, unsigned long> lastLeaseRenewal;

void appendRemoteLog(uint8_t id, const String& logLine) {
    auto& logs = remoteLogsCache[id];
    logs.push_back(logLine);
    if (logs.size() > 50) {
        logs.erase(logs.begin()); // Keep last 50 lines
    }
}

ApiHandler::ApiHandler(NodeRegistry& registry, CapabilityManager& caps, EffectController& controller)
    : _registry(registry), _caps(caps), _effectController(controller), _server(nullptr) {}

void ApiHandler::begin(WebServer* server) {
    _server = server;
    
    _server->on("/api/nodes", HTTP_GET, [this]() { handleApiNodes(); });
    _server->on("/api/system", HTTP_GET, [this]() { handleApiSystem(); });
    _server->on("/api/config", HTTP_GET, [this]() { handleApiConfigGet(); });
    _server->on("/api/config", HTTP_PATCH, [this]() { handleApiConfigPatch(); });
    _server->on("/api/config/mqtt_password", HTTP_PUT, [this]() { handleApiConfigPassword(); });
    
    _server->on(UriBraces("/api/nodes/{}/logs"), HTTP_GET, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        handleApiLogs(id);
    });

    _server->on(UriBraces("/api/nodes/{}/light"), HTTP_POST, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        handleApiLight(id);
    });
    
    _server->on(UriBraces("/api/nodes/{}/sensors"), HTTP_GET, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        handleApiSensors(id);
    });

    _server->on(UriBraces("/api/nodes/{}/capabilities"), HTTP_GET, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        handleApiCapabilities(id);
    });

    _server->on(UriBraces("/api/nodes/{}"), HTTP_PATCH, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        if (_server->hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, _server->arg("plain"));
            if (!err && doc.containsKey("name")) {
                String newName = doc["name"].as<String>();
                iceLog.printf("API [PATCH]: Renamed Node %d to '%s'\n", id, newName.c_str());
                _registry.setNodeName(id, newName);
                _server->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
        _server->send(400, "application/json", "{\"error\":\"Invalid request\"}");
    });

    _server->on(UriBraces("/api/nodes/{}"), HTTP_DELETE, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        iceLog.printf("API [DELETE]: Unregistered Node %d\n", id);
        _registry.unregisterNode(id);
        _server->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    _server->on(UriBraces("/api/nodes/{}/reboot"), HTTP_POST, [this]() {
        uint8_t id = _server->pathArg(0).toInt();
        iceLog.printf("API [POST]: Reboot requested for Node %d\n", id);
        _effectController.rebootNode(id);
        _server->send(200, "application/json", "{\"status\":\"ok\"}");
    });
}

void ApiHandler::handleApiNodes() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    JsonObject hub = arr.add<JsonObject>();
    hub["id"] = 0;
    hub["name"] = "Hub";
    hub["isOnline"] = true;

    std::vector<NodeInfo> nodes = _registry.getKnownNodes();
    for (const auto& node : nodes) {
        JsonObject n = arr.add<JsonObject>();
        n["id"] = node.id;
        n["name"] = node.name;
        n["isOnline"] = node.isOnline;
        n["isConfigured"] = node.isConfigured;
    }

    String response;
    serializeJson(doc, response);
    _server->send(200, "application/json", response);
}

void ApiHandler::handleApiSystem() {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    
    time_t nowTime;
    time(&nowTime);
    struct tm timeinfo;
    
    if (nowTime > 1577836800) { // Post 2020 means NTP has synchronized
        localtime_r(&nowTime, &timeinfo);
        char tsBuf[64];
        strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        doc["time"] = tsBuf;
    } else {
        doc["time"] = "Not Synchronized";
    }
    
    String response;
    serializeJson(doc, response);
    _server->send(200, "application/json", response);
}

void ApiHandler::handleApiConfigGet() {
    SystemConfig config;
    JsonDocument doc;
    doc["hostname"] = config.getHostname();
    doc["tz"] = config.getTimezone();
    doc["mqtt_server"] = config.getMqttServer();
    doc["mqtt_user"] = config.getMqttUser();
    String response;
    serializeJson(doc, response);
    _server->send(200, "application/json", response);
}

void ApiHandler::handleApiConfigPatch() {
    if (_server->hasArg("plain")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server->arg("plain"));
        if (!err) {
            SystemConfig config;
            if (doc.containsKey("hostname")) config.setHostname(doc["hostname"].as<String>());
            if (doc.containsKey("tz")) config.setTimezone(doc["tz"].as<String>());
            if (doc.containsKey("mqtt_server")) config.setMqttServer(doc["mqtt_server"].as<String>());
            if (doc.containsKey("mqtt_user")) config.setMqttUser(doc["mqtt_user"].as<String>());
            
            iceLog.println("API [PATCH]: Config updated. Rebooting...");
            _server->send(200, "application/json", "{\"status\":\"ok\"}");
            delay(1000);
            ESP.restart();
            return;
        }
    }
    _server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
}

void ApiHandler::handleApiConfigPassword() {
    if (_server->hasArg("plain")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server->arg("plain"));
        if (!err && doc.containsKey("password")) {
            SystemConfig config;
            config.setMqttPassword(doc["password"].as<String>());
            iceLog.println("API [PUT]: MQTT Password updated. Rebooting...");
            _server->send(200, "application/json", "{\"status\":\"ok\"}");
            delay(1000);
            ESP.restart();
            return;
        }
    }
    _server->send(400, "application/json", "{\"error\":\"Invalid JSON or missing password\"}");
}

void ApiHandler::handleApiLogs(uint8_t id) {
    if (id == 0) {
        JsonDocument doc;
        JsonArray logs = doc.to<JsonArray>();
        size_t count = iceLog.getCurrentLogCount();
        for (size_t i = 0; i < count; i++) {
            size_t offset = count - 1 - i; // Oldest first
#ifdef USE_DYNAMIC_LOGS
            logs.add(iceLog.getLogEntryFormatted(offset));
#else
            logs.add(iceLog.getLogEntry(offset));
#endif
        }
        String response;
        serializeJson(doc, response);
        _server->send(200, "application/json", response);
    } else {
        // Remote Logs - Lease Renewal Check
        if (lastLeaseRenewal.find(id) == lastLeaseRenewal.end() || millis() - lastLeaseRenewal[id] > 10000) {
            lastLeaseRenewal[id] = millis();
            iceLog.printf("API: Renewing log stream lease for Node %d\n", id);
            ConfigPayload cfg;
            cfg.command = 2; // Stream Logs
            transport.sendMessage(id, PACKET_CONFIG, &cfg, sizeof(cfg));
        }

        JsonDocument doc;
        JsonArray logs = doc.to<JsonArray>();
        if (remoteLogsCache.count(id) && !remoteLogsCache[id].empty()) {
            for (const String& line : remoteLogsCache[id]) {
                logs.add(line);
            }
        } else {
            logs.add("[Waiting for remote logs...]");
        }
        
        String response;
        serializeJson(doc, response);
        _server->send(200, "application/json", response);
    }
}

void ApiHandler::handleApiLight(uint8_t id) {
    if (_server->hasArg("plain")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _server->arg("plain"));
        if (!err) {
            String details = "";
            if (doc.containsKey("brightness")) {
                uint8_t bri = doc["brightness"].as<uint8_t>();
                details += " bri=" + String(bri);
                _effectController.setBrightness(id, bri);
            }
            if (doc.containsKey("color")) {
                JsonObject color = doc["color"];
                uint8_t r = color["r"].as<uint8_t>();
                uint8_t g = color["g"].as<uint8_t>();
                uint8_t b = color["b"].as<uint8_t>();
                details += " rgb=" + String(r) + "," + String(g) + "," + String(b);
                _effectController.setManualColor(id, r, g, b);
            }
            if (doc.containsKey("effect")) {
                String effectName = doc["effect"].as<String>();
                details += " effect=" + effectName;
                // Resolve Mode Name to ID to leverage existing C++ capabilities mapping
                String capsJson = _caps.getCapabilities(id);
                if (capsJson.length() > 0) {
                    JsonDocument capDoc;
                    deserializeJson(capDoc, capsJson);
                    JsonArray modes = capDoc["modes"];
                    for (int i = 0; i < modes.size(); i++) {
                        if (modes[i].as<String>() == effectName) {
                            _effectController.setEffect(id, (uint8_t)i);
                            break;
                        }
                    }
                }
            }
            
            iceLog.printf("API [POST]: Light control for Node %d:%s\n", id, details.c_str());
            _server->send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    _server->send(400, "application/json", "{\"error\":\"Bad request or invalid JSON\"}");
}

void ApiHandler::handleApiSensors(uint8_t id) {
    JsonDocument doc;
    if (nodeSensorCache[id].valid) {
        unsigned long age = millis() - nodeSensorCache[id].lastUpdate;
        doc["valid"] = true;
        doc["stale"] = (age > 300000); // Exceeds 5 minutes
        doc["temperature"] = nodeSensorCache[id].temperature;
        doc["humidity"] = nodeSensorCache[id].humidity;
    } else {
        doc["valid"] = false;
    }
    String response;
    serializeJson(doc, response);
    _server->send(200, "application/json", response);
}

void ApiHandler::handleApiCapabilities(uint8_t id) {
    String caps = _caps.getCapabilities(id);
    if (caps.length() > 0) _server->send(200, "application/json", caps);
    else _server->send(200, "application/json", "{}");
}

#endif