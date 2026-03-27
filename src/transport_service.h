#ifndef TRANSPORT_SERVICE_H
#define TRANSPORT_SERVICE_H

#include <Arduino.h>
#include "ice_protocol.h"
#include "ice_radio.h"
#include "ice_service.h"

class TransportService : public IceService {
public:
    // High-level Data Transfer Object (DTO)
    // This abstracts away the underlying packet structure (Single vs Multipart)
    struct Message {
        uint8_t sourceId;
        uint8_t targetId;
        uint16_t type;      // Maps to PacketType (0-255) or Custom Multipart Types (>255)
        const uint8_t* data;
        size_t length;
    };

    // Callback for when a message is fully received/reassembled
    using MessageCallback = void (*)(const Message&, void* context);

    // Filter callback for Smart Broadcasts.
    // Returns true if 'nodeId' is interested in 'msgType'.
    using BroadcastFilter = bool (*)(uint8_t nodeId, uint16_t msgType, void* context);

    TransportService(IceRadio& radio);

    // Initialize with the local Node ID (0 for Hub, 1-254 for Remote)
    void begin(uint8_t nodeId);
    
    // Maintenance loop (handles reassembly timeouts)
    void loop() override;

    // Check if the async multipart queue is currently transmitting
    bool isBusy() const;

    // --- Egress (Sending) ---
    // Central routing method.
    // - If targetId == _nodeId: Dispatches to local handler immediately (Loopback).
    // - If targetId == 255 (Broadcast): Iterates 0..254, calls BroadcastFilter, sends unicast if true.
    // - If targetId != _nodeId: Serializes and transmits via Radio (handling fragmentation).
    bool sendMessage(uint8_t targetId, uint16_t type, const void* data, size_t length);

    // --- Ingress (Receiving) ---
    // Register a listener for fully assembled messages
    void onMessageReceived(MessageCallback callback, void* context);

    // Inject the logic to determine if a node cares about a broadcast
    void setBroadcastFilter(BroadcastFilter filter, void* context);

    // Feed raw packets from the Radio into the Transport Layer
    // This is typically called from the Radio's callback
    void processPacket(const IcePacket& packet);

private:
    IceRadio& _radio;
    uint8_t _nodeId;
    MessageCallback _callback;
    void* _callbackContext;
    BroadcastFilter _broadcastFilter;
    void* _filterContext;

    // Multipart Reassembly State
    uint8_t* _mpBuffer;
    size_t _mpExpectedSize;
    size_t _mpReceivedSize;
    uint8_t _mpSenderId;
    uint16_t _mpType;
    unsigned long _mpLastChunkTime;

    // Internal Helpers
    void handleMultipartPacket(const IcePacket& packet);
    void dispatch(uint8_t source, uint8_t target, uint16_t type, const uint8_t* data, size_t length);
    bool sendFragmented(uint8_t targetId, uint16_t type, const uint8_t* data, size_t length);
};

#endif