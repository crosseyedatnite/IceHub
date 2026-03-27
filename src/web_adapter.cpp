#include "web_adapter.h"
#ifdef DEVICE_ROLE_HUB
#include "IceHubLog.h"
extern IceHubLog iceLog; // Access the global logger created in main.cpp

WebAdapter::WebAdapter(NodeRegistry& registry, CapabilityManager& caps, EffectController& controller)
    : _server(80), _uiHandler(registry), _apiHandler(registry, caps, controller) {}

void WebAdapter::begin() {
    _uiHandler.begin(&_server);
    _apiHandler.begin(&_server);
    
    _server.begin();
    iceLog.println("Web Adapter started");
}

void WebAdapter::loop() {
    _server.handleClient();
}
#endif