#include "home_assistant_service.h"
#ifdef DEVICE_ROLE_HUB
#include <ArduinoJson.h>
#include "ice_protocol.h" // For DisplayMode enum

#include "IceHubLog.h"
extern IceHubLog iceLog;

static MqttTransport* globalMqtt = nullptr;
static NodeRegistry* globalRegistry = nullptr;

HomeAssistantService::HomeAssistantService(MqttTransport& mqtt, NodeRegistry& registry, CapabilityManager& caps, EffectController& effects)
    : _mqtt(mqtt), _registry(registry), _caps(caps), _effects(effects) {}

void HomeAssistantService::begin() {
    globalMqtt = &_mqtt;
    globalRegistry = &_registry;

    // Subscribe to all light set commands
    _mqtt.onMessage([](const char* topic, const uint8_t* payload, size_t length, void* ctx) {
        static_cast<HomeAssistantService*>(ctx)->handleMqttMessage(topic, payload, length);
    }, this);

    // Listen for new capabilities to trigger discovery
    _caps.onCapabilitiesUpdated([](uint8_t nodeId, void* ctx) {
        static_cast<HomeAssistantService*>(ctx)->publishDiscovery(nodeId);
    }, this);

    // Handle MQTT Connection (Subscribe & Flush Discovery)
    _mqtt.onConnect([](void* ctx) {
        static_cast<HomeAssistantService*>(ctx)->handleMqttConnect();
    }, this);

    // Listen for state changes (from Web UI or internal logic) to update HA
    _effects.onStateChanged([](uint8_t nodeId, const EffectPayload& payload, void* ctx) {
        static_cast<HomeAssistantService*>(ctx)->publishState(nodeId, payload);
    }, this);
}

void HomeAssistantService::loop() {
    // Periodic state updates could go here
}

void HomeAssistantService::handleMqttMessage(const char* topic, const uint8_t* payload, size_t length) {
    // Topic format: home/lights/[FRIENDLY_NAME]/set
    iceLog.printf("HomeAssistantService: Received MQTT message on topic %s\n", topic);
    String topicStr = String(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    int secondLastSlash = topicStr.lastIndexOf('/', lastSlash - 1);
    String targetName = topicStr.substring(secondLastSlash + 1, lastSlash);

    // Resolve Name -> ID
    uint8_t targetId = 0;
    std::vector<NodeInfo> nodes = _registry.getKnownNodes();
    for (const auto& node : nodes) {
        String safeName = node.name;
        safeName.toLowerCase();
        safeName.replace(" ", "_");
        if (safeName == targetName) {
            targetId = node.id;
            break;
        }
    }

    if (targetId == 0) return;

    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload, length);

    if (doc.containsKey("state")) {
        if (strcmp(doc["state"], "OFF") == 0) {
            _effects.setEffect(targetId, OFF); 
        } else if (strcmp(doc["state"], "ON") == 0) {
            // If turning ON and no other effect/color is specified, ensure we aren't in OFF mode.
            if (!doc.containsKey("effect") && !doc.containsKey("color")) {
                _effects.setEffect(targetId, MANUAL_SOLID);
            }
        }
    }

    if (doc.containsKey("brightness")) {
        _effects.setBrightness(targetId, doc["brightness"]);
    }

    if (doc.containsKey("color")) {
        _effects.setManualColor(targetId, doc["color"]["r"], doc["color"]["g"], doc["color"]["b"]);
    }

    if (doc.containsKey("effect")) {
        String effectName = doc["effect"];
        // We need to map string -> ID. 
        // For now, we rely on the WebAdapter/EffectController logic or CapabilityManager
        // Ideally EffectController should have a helper for this.
        // Let's do a quick lookup in the node's capabilities
        String caps = _caps.getCapabilities(targetId);
        if (caps.length() > 0) {
            StaticJsonDocument<1024> capDoc;
            deserializeJson(capDoc, caps);
            JsonArray modes = capDoc["modes"];
            for(int i=0; i<modes.size(); i++) {
                if (modes[i].as<String>() == effectName) {
                    _effects.setEffect(targetId, i);
                    break;
                }
            }
        }
    }
}

void HomeAssistantService::publishDiscovery(uint8_t nodeId) {
    iceLog.printf("Publishing Home Assistant discovery for Node %d\n", nodeId);
    if (!_mqtt.isConnected()) return;

    String caps = _caps.getCapabilities(nodeId);
    if (caps.length() == 0) return; // No capabilities known yet

    StaticJsonDocument<1024> capDoc;
    deserializeJson(capDoc, caps);


    String friendlyName = _registry.getNodeName(nodeId);
    if (friendlyName.length() == 0) friendlyName = "Ice Node " + String(nodeId);

    String safeName = friendlyName;
    safeName.toLowerCase();
    safeName.replace(" ", "_");

       // 1. Publish Light Discovery (if LED capabilities exist)
    if (capDoc.containsKey("leds") && capDoc["leds"].as<int>() > 0) {
        String discoveryTopic = String("homeassistant/light/") + safeName + "/config";
        
        StaticJsonDocument<1024> doc;
        doc["name"] = friendlyName;
        doc["unique_id"] = "ice_node_" + String(nodeId) + "_light";
        doc["state_topic"] = String("home/lights/") + safeName + "/state";
        doc["command_topic"] = String("home/lights/") + safeName + "/set";
        doc["schema"] = "json";
        doc["brightness"] = true;
        doc["effect"] = true;
        JsonArray colorModes = doc.createNestedArray("supported_color_modes");
        colorModes.add("rgb");
        doc["effect_list"] = capDoc["modes"];

        JsonObject device = doc.createNestedObject("device");
        device["identifiers"][0] = "ice_node_" + String(nodeId);
        device["name"] = friendlyName;
        device["model"] = "IceHub Node";
        char buffer[1024];
        serializeJson(doc, buffer);
        _mqtt.publish(discoveryTopic.c_str(), buffer, true);
    }

      // 2. Publish Sensor / Binary Sensor Discovery
    if (capDoc.containsKey("capabilities")) {
        JsonArray sensors = capDoc["capabilities"];
        for (JsonVariant v : sensors) {
            String sensorType = v.as<String>();
            String component = (sensorType == "motion" || sensorType == "presence") ? "binary_sensor" : "sensor";
            String discoveryTopic = String("homeassistant/") + component + "/" + safeName + "_" + sensorType + "/config";
            
            StaticJsonDocument<512> doc;
            doc["name"] = friendlyName + " " + sensorType;
            doc["unique_id"] = "ice_node_" + String(nodeId) + "_" + sensorType;
            doc["state_topic"] = String("home/sensors/") + safeName + "/state";
            doc["value_template"] = String("{{ value_json.") + sensorType + " }}";
            
            if (sensorType == "temperature") {
                doc["device_class"] = "temperature";
                doc["unit_of_measurement"] = "°C";
            } else if (sensorType == "humidity") {
                doc["device_class"] = "humidity";
                doc["unit_of_measurement"] = "%";
            } else if (sensorType == "motion" || sensorType == "presence") {
                doc["device_class"] = "motion";
                doc["payload_on"] = "ON";
                doc["payload_off"] = "OFF";
            }

            JsonObject device = doc.createNestedObject("device");
            device["identifiers"][0] = "ice_node_" + String(nodeId);
            device["name"] = friendlyName;
            device["model"] = "IceHub Node";

            char buffer[512];
            serializeJson(doc, buffer);
            _mqtt.publish(discoveryTopic.c_str(), buffer, true);
        }
    }  
}

void HomeAssistantService::handleMqttConnect() {
    iceLog.println("HomeAssistantService: MQTT Connected. Subscribing and publishing pending discoveries...");
    _mqtt.subscribe("home/lights/+/set");

    // Flush discovery for any nodes we already know about
    std::vector<NodeInfo> nodes = _registry.getKnownNodes();
    for (const auto& node : nodes) {
        if (_caps.hasCapabilities(node.id)) {
            publishDiscovery(node.id);
        }
    }
}

void HomeAssistantService::publishState(uint8_t nodeId, const EffectPayload& payload) {
    if (!_mqtt.isConnected()) return;

    String friendlyName = _registry.getNodeName(nodeId);
    if (friendlyName.length() == 0) return;

    String safeName = friendlyName;
    safeName.toLowerCase();
    safeName.replace(" ", "_");

    String stateTopic = String("home/lights/") + safeName + "/state";
    
    StaticJsonDocument<512> doc;
    
    // Handle Mode / State
    if (payload.mode != 255) {
        if (payload.mode == OFF) {
            doc["state"] = "OFF";
        } else {
            doc["state"] = "ON";
            if (payload.mode == MANUAL_SOLID) {
                doc["color_mode"] = "rgb";
                JsonObject color = doc.createNestedObject("color");
                color["r"] = payload.r;
                color["g"] = payload.g;
                color["b"] = payload.b;
            } else {
                // Lookup effect name from capabilities
                String caps = _caps.getCapabilities(nodeId);
                if (caps.length() > 0) {
                    StaticJsonDocument<1024> capDoc;
                    deserializeJson(capDoc, caps);
                    JsonArray modes = capDoc["modes"];
                    if (payload.mode < modes.size()) {
                        doc["effect"] = modes[payload.mode];
                    }
                }
            }
        }
    }

    if (payload.brightness != 255) {
        doc["brightness"] = payload.brightness;
    }

    char buffer[512];
    serializeJson(doc, buffer);
    _mqtt.publish(stateTopic.c_str(), buffer, true);
}

void publishNodeSensorDataHA(uint8_t nodeId, float temp, float hum) {
    if (!globalMqtt || !globalMqtt->isConnected() || !globalRegistry) return;

    String friendlyName = globalRegistry->getNodeName(nodeId);
    if (friendlyName.length() == 0) friendlyName = "Ice Node " + String(nodeId);

    String safeName = friendlyName;
    safeName.toLowerCase();
    safeName.replace(" ", "_");

    String stateTopic = String("home/sensors/") + safeName + "/state";
    
    StaticJsonDocument<256> doc;
    doc["temperature"] = temp;
    doc["humidity"] = hum;

    char buffer[256];
    serializeJson(doc, buffer);
    globalMqtt->publish(stateTopic.c_str(), buffer, true); // Retain sensor data so HA sees it immediately on restart
}
#endif