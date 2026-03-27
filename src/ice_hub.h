#ifndef ICE_HUB_H
#define ICE_HUB_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <WiFi.h>
#include "transport_service.h"
#include "node_registry.h"
#include "capability_manager.h"
#include "web_adapter.h"
#include "effect_controller.h"
#include "ice_service.h"
#include "system_config.h"

class IceHub : public IceService {
public:
    IceHub(SystemConfig& config, TransportService& transport, NodeRegistry& registry, CapabilityManager& caps, WebAdapter& web, EffectController& effects);
    
    void begin();
    void loop() override;
    
    // Internal Event Handlers (Public for callback wrapper)
    void handleMessage(const TransportService::Message& msg);
    CapabilityManager& _caps; // Public for callback wrapper

private:
    SystemConfig& _config;
    TransportService& _transport;
    NodeRegistry& _registry;
    WebAdapter& _web;
    EffectController& _effects;
    
    void setupWiFi();
    void performSerialSetup();
    void checkSerial();
    bool connectWithTimeout();

    String _ssid;
    String _password;
    String _hostname;
    
    bool _awaitingResetConfirmation;
    unsigned long _resetRequestTime;
    
    uint8_t _discoveryIndex;
    unsigned long _lastDiscoveryTime;
};

#endif // DEVICE_ROLE_HUB
#endif