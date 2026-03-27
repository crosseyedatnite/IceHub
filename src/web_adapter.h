#ifndef WEB_ADAPTER_H
#define WEB_ADAPTER_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <WebServer.h>
#include "ui_handler.h"
#include "api_handler.h"
#include "node_registry.h"
#include "capability_manager.h"
#include "effect_controller.h"

class WebAdapter {
public:
    WebAdapter(NodeRegistry& registry, CapabilityManager& caps, EffectController& controller);
    void begin();
    void loop();

private:
    WebServer _server;
    UiHandler _uiHandler;
    ApiHandler _apiHandler;
};

#endif // DEVICE_ROLE_HUB
#endif