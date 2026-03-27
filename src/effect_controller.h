#ifndef EFFECT_CONTROLLER_H
#define EFFECT_CONTROLLER_H

#include <Arduino.h>
#include "transport_service.h"
#include "ice_protocol.h"
#ifdef HAS_LEDS
#include "ice_effects.h" // For DisplayMode enum
#endif

class EffectController {
public:
    // Hub Constructor (Sender)
    EffectController(TransportService& transport);
    
    #ifdef HAS_LEDS
    // Remote Constructor (Receiver)
    EffectController(IceEffects& effects);
    EffectController(TransportService& transport, IceEffects& effects);
    void handleState(const EffectPayload& payload);
    #endif

    void setEffect(uint8_t nodeId, uint8_t modeId);
    void setBrightness(uint8_t nodeId, uint8_t brightness);
    void setManualColor(uint8_t nodeId, uint8_t r, uint8_t g, uint8_t b);
    void rebootNode(uint8_t nodeId);

    // Callback for state changes (sent or received)
    using StateCallback = void (*)(uint8_t nodeId, const EffectPayload& payload, void* context);
    void onStateChanged(StateCallback callback, void* context);

private:
    TransportService* _transport;
    StateCallback _stateCallback;
    void* _stateContext;
    #ifdef HAS_LEDS
    IceEffects* _effects;
    #endif
};

#endif