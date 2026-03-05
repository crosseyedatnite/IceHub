#ifndef ICE_RADIO_H
#define ICE_RADIO_H

#include <Arduino.h>
#include <RF24.h>
#include "ice_protocol.h"

class IceRadio {
public:
    IceRadio(uint8_t ce_pin, uint8_t csn_pin);
    bool begin(uint8_t nodeId);
    void update();
    bool send(uint8_t targetId, const IcePacket& packet);
    
    // Callback for received packets
    typedef void (*RadioCallback)(const IcePacket&);
    void onPacketReceived(RadioCallback callback);

    bool sendMultipart(uint8_t targetId, uint16_t type, const void* data, size_t length);

private:
    RF24 _radio;
    uint8_t _nodeId;
    RadioCallback _callback;
};

#endif