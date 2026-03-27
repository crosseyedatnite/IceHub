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
    PACKET_REGISTER_ACK = 11, // Hub -> Node: "You are ID X"
    PACKET_REBOOT = 12,       // Hub -> Node: "Restart yourself"
    PACKET_INVALIDATE = 13    // Hub -> Node: "You are not registered, wipe EEPROM"
};

// Shared Display Modes (Moved from IceEffects so Hub knows them)
enum DisplayMode { 
    RAINBOW_PULSE = 0, 
    TRAIL_SPARK = 1, 
    PALETTE_WAVE = 2, 
    CONFETTI = 3, 
    JUGGLE = 4, 
    JITTER = 5, 
    CRACKLE = 6, 
    FIREWORK = 7, 
    BREATHE = 8, 
    SCANNER = 9, 
    TWINKLE = 10, 
    METEOR = 11, 
    MANUAL_SOLID = 12, 
    OFF = 13 
};

// --- Payload Definitions ---

struct __attribute__((packed)) EffectPayload {
    uint8_t mode;       // Mapped to DisplayMode in IceEffects
    uint8_t brightness; // 0-255
    uint8_t r, g, b;    // Color
    uint8_t speed;      // Animation speed
};

struct __attribute__((packed)) SensorPayload {
    int16_t temperature; // x100
    uint16_t humidity;   // x100
    uint16_t batteryMv;
};

struct __attribute__((packed)) ConfigPayload {
    uint8_t command;    // 1=GetCaps
    uint8_t reserved[7];
};

struct __attribute__((packed)) RegistrationPayload {
    uint32_t nonce;       // Random ID to match Request/Ack
    uint8_t deviceType;   // 1=LED, 2=Sensor, 3=Hybrid
    uint8_t capabilities; // Bitmask (0x01=HasLEDs, 0x02=HasTemp)
    uint8_t assignedId;   // Used in ACK
    uint32_t token;       // Security token
};

struct __attribute__((packed)) PingPayload {
    uint32_t token;
};

struct __attribute__((packed)) MultipartPayload {
    uint16_t segmentId;
    union {
        uint8_t data[27];
        struct __attribute__((packed)) {
            uint16_t totalSize;
            uint16_t type;
        } header;
    };
};

struct __attribute__((packed)) IcePacket {
    uint8_t targetID;    // 0=Hub, 1-254=Node ID, 255=Broadcast/Unassigned
    uint8_t senderID;    // Who sent this?
    uint8_t msgType;     // PacketType enum

    union {
        EffectPayload effect;
        SensorPayload sensor;
        ConfigPayload config;
        RegistrationPayload registration;
        MultipartPayload multipart;
        PingPayload ping;
    } payload;
};

#endif