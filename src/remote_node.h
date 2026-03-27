#ifndef REMOTE_NODE_H
#define REMOTE_NODE_H

#include <Arduino.h>
#include "transport_service.h"
#include "ice_service.h"
#ifdef HAS_LEDS
#include "effect_controller.h"
#include "ice_effects.h"
#endif

class RemoteNode : public IceService {
public:
    #ifdef HAS_LEDS
    RemoteNode(TransportService& transport, EffectController& controller, IceEffects& effects);
    #else
    RemoteNode(TransportService& transport);
    #endif

    void begin();
    void loop() override;

    // Public for callback wrapper
    void handleMessage(const TransportService::Message& msg);

private:
    TransportService& _transport;
    #ifdef HAS_LEDS
    EffectController& _controller;
    IceEffects& _effects;
    #endif
    
    uint8_t _myNodeId;
    uint8_t _activeRadioId;
    uint32_t _myToken;
    uint32_t _currentNonce;
    unsigned long _lastRegAttempt;
    unsigned long _regAttemptCount;
    unsigned long _lastStatusTime;

    void attemptRegistration();
    void sendStatus();
};

#endif