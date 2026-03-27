#include "effect_controller.h"

#include "IceHubLog.h"
extern IceHubLog iceLog;

EffectController::EffectController(TransportService& transport) 
    : _transport(&transport), _stateCallback(nullptr), _stateContext(nullptr)
#ifdef HAS_LEDS
, _effects(nullptr) 
#endif
{}

#ifdef HAS_LEDS
EffectController::EffectController(IceEffects& effects) 
    : _transport(nullptr), _stateCallback(nullptr), _stateContext(nullptr), _effects(&effects) {}
EffectController::EffectController(TransportService& transport, IceEffects& effects) 
    : _transport(&transport), _stateCallback(nullptr), _stateContext(nullptr), _effects(&effects) {}
#endif

void EffectController::setEffect(uint8_t nodeId, uint8_t modeId) {
    EffectPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.mode = modeId;
    payload.brightness = 255; // 255 = No Change
    
    if (_transport) _transport->sendMessage(nodeId, PACKET_STATE, &payload, sizeof(payload));
    if (_stateCallback) _stateCallback(nodeId, payload, _stateContext);
}

void EffectController::setBrightness(uint8_t nodeId, uint8_t brightness) {
    EffectPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.mode = 255; // 255 = No Change
    payload.brightness = brightness;
    
    if (_transport) _transport->sendMessage(nodeId, PACKET_STATE, &payload, sizeof(payload));
    if (_stateCallback) _stateCallback(nodeId, payload, _stateContext);
}

void EffectController::setManualColor(uint8_t nodeId, uint8_t r, uint8_t g, uint8_t b) {
    EffectPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.mode = (uint8_t)MANUAL_SOLID;
    payload.brightness = 255; // 255 = No Change
    payload.r = r;
    payload.g = g;
    payload.b = b;
    
    if (_transport) _transport->sendMessage(nodeId, PACKET_STATE, &payload, sizeof(payload));
    if (_stateCallback) _stateCallback(nodeId, payload, _stateContext);
}

void EffectController::rebootNode(uint8_t nodeId) {
    // Send empty packet with REBOOT type
    if (_transport) _transport->sendMessage(nodeId, PACKET_REBOOT, nullptr, 0);
}

void EffectController::onStateChanged(StateCallback callback, void* context) {
    _stateCallback = callback;
    _stateContext = context;
}

#ifdef HAS_LEDS
void EffectController::handleState(const EffectPayload& payload) {
    if (!_effects) return;

    iceLog.printf(F("Fx: M%d B%d C%d,%d,%d\n"), 
        payload.mode, payload.brightness, payload.r, payload.g, payload.b);

    if (payload.mode != 255) {
        DisplayMode mode = static_cast<DisplayMode>(payload.mode);
        if (mode == MANUAL_SOLID) {
            // Only update color if it's not black (0,0,0).
            // This allows switching to MANUAL_SOLID without overwriting the color
            // when the packet was sent via setEffect() (which zeros the payload).
            if (payload.r != 0 || payload.g != 0 || payload.b != 0) {
                _effects->setManualColor(payload.r, payload.g, payload.b);
            }
        }
        _effects->setMode(mode);
    }
    if (payload.brightness != 255) {
        _effects->setBrightness(payload.brightness);
    }
}
#endif