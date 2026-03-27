#ifndef HOME_ASSISTANT_SERVICE_H
#define HOME_ASSISTANT_SERVICE_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include "ice_service.h"
#include "mqtt_transport.h"
#include "node_registry.h"
#include "capability_manager.h"
#include "effect_controller.h"

class HomeAssistantService : public IceService {
public:
    HomeAssistantService(MqttTransport& mqtt, NodeRegistry& registry, CapabilityManager& caps, EffectController& effects);
    
    void begin();
    void loop() override;

    // Triggered by IceHub when a node is ready
    void publishDiscovery(uint8_t nodeId);

    // Triggered by EffectController when state changes
    void publishState(uint8_t nodeId, const EffectPayload& payload);

private:
    MqttTransport& _mqtt;
    NodeRegistry& _registry;
    CapabilityManager& _caps;
    EffectController& _effects;

    void handleMqttMessage(const char* topic, const uint8_t* payload, size_t length);
    void handleMqttConnect();
};

#endif // DEVICE_ROLE_HUB
#endif