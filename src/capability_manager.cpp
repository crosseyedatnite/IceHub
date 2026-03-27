#include "capability_manager.h"
#ifdef DEVICE_ROLE_HUB
#include "ice_protocol.h"

#include "IceHubLog.h"
extern IceHubLog iceLog;

CapabilityManager::CapabilityManager() : _callback(nullptr), _callbackContext(nullptr) {}

void CapabilityManager::begin() {
    _capabilities.clear();
}

void CapabilityManager::processCapabilities(uint8_t nodeId, const char* json) {
    _capabilities[nodeId] = String(json);
    iceLog.printf("Hub: Capability Manager sees response message. Updating running cache for Node %d.\n", nodeId);
    iceLog.printf("Hub: Details: %s\n", json);
    
    if (_callback) {
        _callback(nodeId, _callbackContext);
    }
}

String CapabilityManager::getCapabilities(uint8_t nodeId) {
    if (_capabilities.count(nodeId)) {
        return _capabilities[nodeId];
    }
    return "";
}

bool CapabilityManager::hasCapabilities(uint8_t nodeId) {
    return _capabilities.count(nodeId) > 0;
}

bool CapabilityManager::isNodeInterested(uint8_t nodeId, uint16_t msgType) {
    // 1. Basic Interest: All nodes listen to STATE packets (1) and CONFIG packets (3)
    if (msgType == PACKET_STATE || msgType == PACKET_CONFIG) return true;

    // 2. Advanced Interest: Check capabilities
    // For now, we assume if it has capabilities stored, it's a smart node.
    // In the future, we can parse the JSON to see if it has "sensors": ["temp"] etc.
    
    /* Example Future Logic:
    String caps = getCapabilities(nodeId);
    if (caps.length() == 0) return false;
    if (msgType == PACKET_SENSOR && caps.indexOf("temp_display") > 0) return true;
    */

    // Default safe behavior: If we know about the node, send it.
    // We rely on NodeRegistry to tell us if the node exists, but here we just check caps.
    return _capabilities.count(nodeId) > 0;
}

void CapabilityManager::onCapabilitiesUpdated(CapabilitiesCallback callback, void* context) {
    _callback = callback;
    _callbackContext = context;
}
#endif