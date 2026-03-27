#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <WebServer.h>
#include "node_registry.h"

class UiHandler {
public:
    UiHandler(NodeRegistry& registry);
    void begin(WebServer* server);

private:
    NodeRegistry& _registry;
    WebServer* _server;

    void handleRoot();
    void handleLogsPage();
};
#endif // DEVICE_ROLE_HUB
#endif