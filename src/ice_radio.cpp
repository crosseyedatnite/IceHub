#include "ice_radio.h"
#include <SPI.h>

// Base address for the network. 
// Hub listens on this address. Nodes write to this address.
const uint64_t HUB_ADDRESS      = 0xE8E8F0F0E1LL;
const uint64_t NODE_ADDRESS_BASE = 0xE8E8F0F000LL;

IceRadio::IceRadio(uint8_t ce_pin, uint8_t csn_pin) : _radio(ce_pin, csn_pin), _nodeId(0) {}

bool IceRadio::begin(uint8_t nodeId) {
    _nodeId = nodeId;
    
    #ifdef ESP32
    // Explicitly initialize SPI with the user's pin mapping
    SPI.begin(RADIO_SCK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CSN_PIN);
    #endif

    if (!_radio.begin()) {
        Serial.println(F("Radio hardware not responding!"));
        return false;
    }

    _radio.setPALevel(RF24_PA_LOW); // Low power to reduce interference/heat
    _radio.setDataRate(RF24_1MBPS);
    _radio.setChannel(100);         // Channel 100 (Above most WiFi)
    _radio.enableDynamicPayloads();
    _radio.enableAckPayload();      // Allow Hub to send data back in ACK
    
    if (_nodeId == 0) {
        // Hub Role: Listen on the main pipe
        _radio.openReadingPipe(1, HUB_ADDRESS);
    } else {
        // Node Role: Listen on my specific address
        _radio.openReadingPipe(1, NODE_ADDRESS_BASE + _nodeId);
    }

    _radio.startListening();
    Serial.print(F("Radio initialized. Node ID: "));
    Serial.println(_nodeId);
    return true;
}

void IceRadio::update() {

    if (_radio.available()) {
        uint8_t len = _radio.getDynamicPayloadSize();
        // Accept any valid packet size (Header is 3 bytes, Max is 32)
        if (len >= 3 && len <= 32) {
            IcePacket packet;
            memset(&packet, 0, sizeof(IcePacket)); // Zero-pad to handle smaller packets safely
            _radio.read(&packet, len);
            if (_callback) {
                _callback(packet);
            }
        } else {
            // Flush invalid packet
            uint8_t dump[32];
            _radio.read(dump, len);
            // Use '\r' to return to start of line and overwrite (prevents scrolling spam)
            Serial.print(F("\r[Radio] Noise/Invalid Packet (len="));
            Serial.print(len);
            Serial.print(F(", Expected="));
            Serial.print(sizeof(IcePacket));
            Serial.println(F(")   ")); // Use println to persist error log
        }
    } 
}

bool IceRadio::send(uint8_t targetId, const IcePacket& packet) {
    _radio.stopListening();
    uint64_t addr = (targetId == 0) ? HUB_ADDRESS : (NODE_ADDRESS_BASE + targetId);
    _radio.openWritingPipe(addr);
    bool result = _radio.write(&packet, sizeof(IcePacket));
    _radio.startListening();
    return result;
}

bool IceRadio::sendMultipart(uint8_t targetId, uint16_t type, const void* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    // Calculate segments: (length + 26) / 27 is integer ceil(length/27)
    uint16_t totalSegments = (length + 26) / 27; 
    
    // 1. Send Header (Segment 0)
    IcePacket header;
    memset(&header, 0, sizeof(header));
    header.targetID = targetId;
    header.senderID = _nodeId;
    header.msgType = PACKET_MULTIPART;
    header.payload.multipart.segmentId = 0;
    
    // Store total size in first 2 bytes of data for the receiver to allocate
    header.payload.multipart.header.totalSize = (uint16_t)length;
    header.payload.multipart.header.type = type;
    
    if (!send(targetId, header)) return false;
    
    // 2. Send Data Segments
    for (uint16_t i = 1; i <= totalSegments; i++) {
        IcePacket chunk;
        memset(&chunk, 0, sizeof(chunk));
        chunk.targetID = targetId;
        chunk.senderID = _nodeId;
        chunk.msgType = PACKET_MULTIPART;
        chunk.payload.multipart.segmentId = i;
        
        size_t offset = (i - 1) * 27;
        size_t chunkLen = length - offset;
        if (chunkLen > 27) chunkLen = 27;
        
        memcpy(chunk.payload.multipart.data, bytes + offset, chunkLen);
        
        if (!send(targetId, chunk)) return false;
        delay(5); // Small delay to prevent flooding the receiver's buffer
    }
    return true;
}

void IceRadio::onPacketReceived(RadioCallback callback) {
    _callback = callback;
}