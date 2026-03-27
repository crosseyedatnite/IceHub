#ifndef CAPABILITY_MANAGER_H
#define CAPABILITY_MANAGER_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <map>
#include <ArduinoJson.h>

class CapabilityManager {
public:
    CapabilityManager();
    void begin();

    // Store received capabilities
    void processCapabilities(uint8_t nodeId, const char* json);

    // Retrieve capabilities (returns empty string if not found)
    String getCapabilities(uint8_t nodeId);
    
    // Lightweight check if capabilities are cached
    bool hasCapabilities(uint8_t nodeId);
    
    // Smart Broadcast Filter
    // Returns true if 'nodeId' is interested in 'msgType'
    bool isNodeInterested(uint8_t nodeId, uint16_t msgType);

    // Callback for when capabilities are updated
    using CapabilitiesCallback = void (*)(uint8_t nodeId, void* context);
    void onCapabilitiesUpdated(CapabilitiesCallback callback, void* context);

private:
    std::map<uint8_t, String> _capabilities;
    CapabilitiesCallback _callback;
    void* _callbackContext;
};

#endif // DEVICE_ROLE_HUB
#endif