#include "remote_node.h"
#include <EEPROM.h>

#include "IceHubLog.h"
extern IceHubLog iceLog;

extern TransportService transport;

#ifdef HAS_LEDS
RemoteNode::RemoteNode(TransportService& transport, EffectController& controller, IceEffects& effects) 
    : _transport(transport), _controller(controller), _effects(effects), _myNodeId(255), 
      _lastRegAttempt(0), _regAttemptCount(0), _lastStatusTime(0) {}
#else
RemoteNode::RemoteNode(TransportService& transport) 
    : _transport(transport), _myNodeId(255), 
      _lastRegAttempt(0), _regAttemptCount(0), _lastStatusTime(0) {}
#endif

static void logStreamCb(const char* logLine, uint8_t targetId, void* context) {
    transport.sendMessage(targetId, 2, logLine, strlen(logLine)); // Type 2 = Logs
}

void RemoteNode::begin() {
    
    EEPROM.get(0, _myNodeId);
    EEPROM.get(1, _myToken); // Load 4-byte token from EEPROM addresses 1-4
    if (_myNodeId == 0) _myNodeId = 255; // Safety check
    
    iceLog.printf(F("RemoteNode: Loaded ID from EEPROM: %d\n"), _myNodeId);

    // If unassigned, pick a random Ephemeral ID (128-254) for the registration channel
    if (_myNodeId == 255) {
        // Mix analog noise and micros for a semi-random seed
        randomSeed(analogRead(0) ^ micros());
        _activeRadioId = random(128, 255);
    } else {
        _activeRadioId = _myNodeId;
    }

    _transport.begin(_activeRadioId);
    
    // Register callback
    _transport.onMessageReceived([](const TransportService::Message& msg, void* ctx) {
        static_cast<RemoteNode*>(ctx)->handleMessage(msg);
    }, this);

    // Register log stream callback
    iceLog.setLogStreamCallback(logStreamCb, nullptr);

    // Send initial Hello if we are already registered
    if (_myNodeId != 255) {
        IcePacket hello;
        memset(&hello, 0, sizeof(hello));
        hello.senderID = _myNodeId;
        hello.targetID = 0;
        hello.msgType = PACKET_PING;
        // We can use the raw radio via transport, or just send a packet via transport
        // Since PACKET_PING is small, sendMessage works fine.
        
        PingPayload ping;
        ping.token = _myToken;
        _transport.sendMessage(0, PACKET_PING, &ping, sizeof(ping));
    } else {
        // Force immediate registration attempt if unconfigured
        attemptRegistration();
    }
}

void RemoteNode::loop() {
    // Registration Loop
    if (_myNodeId == 255) {
        if (millis() - _lastRegAttempt > 5000) {
            attemptRegistration();
        }
    } else {
        // Periodic Status Update
        if (millis() - _lastStatusTime > 15000) {
            sendStatus();
            _lastStatusTime = millis();
        }
    }
}

void RemoteNode::attemptRegistration() {
    _lastRegAttempt = millis();
    
    _currentNonce = micros(); // Generate a unique nonce for this attempt
    RegistrationPayload reg;
    reg.nonce = _currentNonce;
    #ifdef HAS_LEDS
    iceLog.println(F("Node: Reg as LED Device"));
    reg.deviceType = 1; // LED
    #else
    iceLog.println(F("Node: Reg as Sensor"));
    reg.deviceType = 2; // Sensor
    #endif
    reg.capabilities = 0; 
    reg.assignedId = 0;

    // Send to Hub (0)
    // Note: Registration is special, usually sent to 0 but we might need to broadcast if we don't have an ID?
    // The protocol says Target 0 for Reg Req.
    _transport.sendMessage(0, PACKET_REGISTER_REQ, &reg, sizeof(reg));
    
    iceLog.printf(F("Node: Reg Req Sent #%d\n"), ++_regAttemptCount);
}

void RemoteNode::sendStatus() {
    #if !defined(HAS_Si7021_SENSOR) && !defined(HAS_HLK_LK2420_SENSOR)
    SensorPayload status;
    status.temperature = 0; // Placeholder
    status.humidity = 0;    // Placeholder
    status.batteryMv = 0;   // Placeholder
    _transport.sendMessage(0, PACKET_SENSOR, &status, sizeof(status));
    #endif
}

void RemoteNode::handleMessage(const TransportService::Message& msg) {
    #ifdef HAS_LEDS
    if (msg.type == PACKET_STATE && msg.length >= sizeof(EffectPayload)) {
        const EffectPayload* payload = (const EffectPayload*)msg.data;
        _controller.handleState(*payload);
    }
    #endif

    if (msg.type == PACKET_REGISTER_ACK && _myNodeId == 255 && msg.targetId == _activeRadioId) {
        const RegistrationPayload* payload = (const RegistrationPayload*)msg.data;
        if (payload->nonce == _currentNonce) {
             uint8_t newId = payload->assignedId;
             iceLog.printf(F("Node: Reg ACK. ID:%d\n"), newId);
             EEPROM.write(0, newId);
             EEPROM.put(1, payload->token);
             void (*resetFunc)(void) = 0;
             resetFunc();
        }
    }
    else if (msg.type == PACKET_CONFIG) {
        const ConfigPayload* payload = (const ConfigPayload*)msg.data;
        if (payload->command == 1) { // 1 = Get Capabilities
            iceLog.println(F("Node: Caps Req Rx'd"));
            char buf[192]; // Allocate on stack to reclaim permanent RAM footprint
            #ifdef HAS_LEDS
            iceLog.println(F("Node: Fetching LED caps..."));
            _effects.getCapabilitiesJSON(buf, sizeof(buf));
            #elif defined(HAS_Si7021_SENSOR)
            snprintf(buf, sizeof(buf), "{\"type\":\"sensor\",\"capabilities\":[\"temperature\",\"humidity\"]}");
            #else
            snprintf(buf, sizeof(buf), "{\"leds\":0,\"bri\":0,\"mode\":\"OFF\",\"modes\":[]}");
            #endif
            // Send back as Multipart (Type 1 = JSON Caps)
            iceLog.println(F("Node: Sending Caps JSON"));
            _transport.sendMessage(0, 1, buf, strlen(buf));
        }
        else if (payload->command == 2) { // Stream Logs
            iceLog.println(F("Node: Log Stream Active"));
            if (iceLog.setLogStreamTarget(0, 15000)) { // 15s lease
                // Burst existing logs instantly in chronological order
                size_t count = iceLog.getCurrentLogCount();
                for (size_t i = 0; i < count; i++) {
                    const char* entry = iceLog.getLogEntry(count - 1 - i); // Oldest to newest
                    while (_transport.isBusy()) { _transport.loop(); delay(2); } // Let async chunk finish
                    _transport.sendMessage(0, 2, entry, strlen(entry));
                }
            }
        }
    }
    else if (msg.type == PACKET_INVALIDATE) {
        iceLog.println(F("Node: Invalid ID/Token. Rst"));
        EEPROM.write(0, 255); // Clear ID
        void (*resetFunc)(void) = 0;
        resetFunc();
    }
    else if (msg.type == PACKET_REBOOT) {
        iceLog.println(F("Node: Reboot Cmd Rx'd"));
        void (*resetFunc)(void) = 0;
        resetFunc();
    }
}