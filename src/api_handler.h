#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <WebServer.h>
#include "node_registry.h"
#include "capability_manager.h"
#include "effect_controller.h"

class ApiHandler {
public:
    ApiHandler(NodeRegistry& registry, CapabilityManager& caps, EffectController& controller);
    void begin(WebServer* server);

private:
    NodeRegistry& _registry;
    CapabilityManager& _caps;
    EffectController& _effectController;
    WebServer* _server;

    void handleApiNodes();
    void handleApiSystem();
    void handleApiConfigGet();
    void handleApiConfigPatch();
    void handleApiConfigPassword();
    void handleApiLogs(uint8_t id);
    void handleApiLight(uint8_t id);
    void handleApiSensors(uint8_t id);
    void handleApiCapabilities(uint8_t id);
};
#endif // DEVICE_ROLE_HUB
#endif