#include "node_registry.h"
#ifdef DEVICE_ROLE_HUB

NodeRegistry::NodeRegistry() {
    memset(_sessionSeen, 0, sizeof(_sessionSeen));
    memset(_lastSeen, 0, sizeof(_lastSeen));
    memset(_configured, 0, sizeof(_configured));
}

void NodeRegistry::begin() {
    // Pre-load configuration status to avoid NVS hits in the loop
    if (_prefs.begin("hub_nodes", true)) {
        for (int i = 1; i < 255; i++) {
            if (_prefs.isKey(String(i).c_str())) {
                _configured[i] = true;
            }
        }
        _prefs.end();
    }
    // Ensure namespace exists for writing
    _prefs.begin("hub_nodes", false); 
    _prefs.end();
}

uint8_t NodeRegistry::allocateNextId() {
    _prefs.begin("hub_config", false);
    uint8_t nextId = _prefs.getUChar("next_id", 1);
    
    // Safety check: Skip IDs that are already actively seen or configured
    uint8_t startId = nextId;
    while (_configured[nextId] || _sessionSeen[nextId]) {
        nextId++;
        if (nextId > 127) nextId = 1;
        if (nextId == startId) break; // Avoid infinite loop if all 127 IDs are taken
    }

    // Increment for next time (wrap 1-127)
    uint8_t saveId = nextId + 1;
    if (saveId > 127) saveId = 1;
    _prefs.putUChar("next_id", saveId);
    _prefs.end();
    return nextId;
}

void NodeRegistry::resetNextId() {
    _prefs.begin("hub_config", false);
    _prefs.putUChar("next_id", 1);
    _prefs.end();
}

void NodeRegistry::unregisterNode(uint8_t id) {
    if (id > 0 && id < 255) {
        _prefs.begin("hub_nodes", false);
        _prefs.remove(String(id).c_str());
        _prefs.remove((String(id) + "_t").c_str()); // Also wipe the token
        _prefs.end();
        
        _sessionSeen[id] = false;
        _lastSeen[id] = 0;
        _configured[id] = false;
    }
}

void NodeRegistry::markNodeSeen(uint8_t id) {
    if (id > 0 && id < 255) {
        _sessionSeen[id] = true;
        _lastSeen[id] = millis();
    }
}

bool NodeRegistry::isNodeConfigured(uint8_t id) {
    return _configured[id];
}

void NodeRegistry::setNodeName(uint8_t id, const String& name) {
    _prefs.begin("hub_nodes", false);
    _prefs.putString(String(id).c_str(), name);
    _prefs.end();
    _configured[id] = true;
}

String NodeRegistry::getNodeName(uint8_t id) {
    String name = "";
    if (_prefs.begin("hub_nodes", true)) {
        if (_prefs.isKey(String(id).c_str())) {
            name = _prefs.getString(String(id).c_str(), "");
        }
        _prefs.end();
    }
    return name;
}

void NodeRegistry::setNodeToken(uint8_t id, uint32_t token) {
    _prefs.begin("hub_nodes", false);
    _prefs.putUInt((String(id) + "_t").c_str(), token);
    _prefs.end();
}

uint32_t NodeRegistry::getNodeToken(uint8_t id) {
    uint32_t token = 0;
    if (_prefs.begin("hub_nodes", true)) {
        // Append _t to the ID to create a unique token key
        token = _prefs.getUInt((String(id) + "_t").c_str(), 0); 
        _prefs.end();
    }
    return token;
}

std::vector<NodeInfo> NodeRegistry::getKnownNodes() {
    std::vector<NodeInfo> nodes;
    if (_prefs.begin("hub_nodes", true)) {
        // Scan all valid IDs. 
        // A node is "Known" if we have seen it this session OR it has a saved name.
        for (int i = 1; i < 255; i++) {
            String name = "";
            String key = String(i);
            if (_prefs.isKey(key.c_str())) {
                name = _prefs.getString(key.c_str(), "");
            }
            
            if (_sessionSeen[i] || name.length() > 0) {
                NodeInfo info;
                info.id = i;
                info.name = name;
                info.isOnline = _sessionSeen[i];
                info.isConfigured = (name.length() > 0);
                info.lastSeen = _lastSeen[i];
                nodes.push_back(info);
            }
        }
        _prefs.end();
    }
    return nodes;
}
#endif