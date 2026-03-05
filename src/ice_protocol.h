#ifndef ICE_PROTOCOL_H
#define ICE_PROTOCOL_H

#include <Arduino.h>

// Packet Types
enum PacketType {
    PACKET_PING = 0,
    PACKET_STATE = 1,
    PACKET_SENSOR = 2,
    PACKET_CONFIG = 3,
    PACKET_MULTIPART = 4,     // For sending payloads > packet size
    PACKET_REGISTER_REQ = 10, // Node -> Hub: "I am new"
    PACKET_REGISTER_ACK = 11 // Hub -> Node: "You are ID X"
};

struct __attribute__((packed)) IcePacket {
    uint8_t targetID;    // 0=Hub, 1-254=Node ID, 255=Broadcast/Unassigned
    uint8_t senderID;    // Who sent this?
    uint8_t msgType;     // PacketType enum

    union {
        // Payload for LEDs (Type 1)
        struct __attribute__((packed)) {
            uint8_t mode;       // Mapped to DisplayMode in IceEffects
            uint8_t brightness; // 0-255
            uint8_t r, g, b;    // Color
            uint8_t speed;      // Animation speed
        } effect;

        // Payload for Sensors (Type 2)
        struct __attribute__((packed)) {
            int16_t temperature; // x100
            uint16_t humidity;   // x100
            uint16_t batteryMv;
        } sensor;

        // Payload for Config/Control (Type 3)
        struct __attribute__((packed)) {
            uint8_t command;    // 1=GetCaps
            uint8_t reserved[7];
        } config;

        // Payload for Registration (Type 10/11)
        struct __attribute__((packed)) {
            uint32_t nonce;       // Random ID to match Request/Ack
            uint8_t deviceType;   // 1=LED, 2=Sensor, 3=Hybrid
            uint8_t capabilities; // Bitmask (0x01=HasLEDs, 0x02=HasTemp)
            uint8_t assignedId;   // Used in ACK
        } registration;

        // Payload for multi-part messages, utilizing the full packet (32 bytes max)
        // This allows for transmitting larger data blobs, like JSON descriptions.
        struct __attribute__((packed)) {
            // Segment ID. 0 is the header packet, 1...N are data packets.
            uint16_t segmentId;
            
            // Remaining 27 bytes of the payload (32 total - 3 header - 2 segmentId)
            // For header (segmentId=0): Can contain total size, segment count, etc.
            // For data (segmentId>0): Contains the data chunk.
            union {
                uint8_t data[27];
                struct __attribute__((packed)) {
                    uint16_t totalSize;
                    uint16_t type;      // 1=JSON, 2=Firmware, etc.
                } header;
            };
        } multipart;
    } payload;
};

#endif