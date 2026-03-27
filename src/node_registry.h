#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include <Arduino.h>
#ifdef DEVICE_ROLE_HUB

#include <Preferences.h>
#include <vector>

struct NodeInfo {
    uint8_t id;
    String name;
    bool isOnline;      // Seen in this session
    bool isConfigured;  // Has a name in NVS
    unsigned long lastSeen;
};

class NodeRegistry {
public:
    NodeRegistry();
    void begin();

    // Control Plane: ID Allocation
    uint8_t allocateNextId();
    void resetNextId();
    void unregisterNode(uint8_t id);
    
    // State Management
    void markNodeSeen(uint8_t id);
    bool isNodeConfigured(uint8_t id);
    void setNodeName(uint8_t id, const String& name);
    String getNodeName(uint8_t id);
    void setNodeToken(uint8_t id, uint32_t token);
    uint32_t getNodeToken(uint8_t id);

    // Queries
    std::vector<NodeInfo> getKnownNodes();

private:
    Preferences _prefs;
    bool _sessionSeen[256];
    bool _configured[256];
    unsigned long _lastSeen[256];
};

#endif // DEVICE_ROLE_HUB
#endif