#include "transport_service.h"

#include "IceHubLog.h"
extern IceHubLog iceLog;

TransportService::TransportService(IceRadio& radio) 
    : _radio(radio), _nodeId(0), _callback(nullptr), _callbackContext(nullptr),
      _broadcastFilter(nullptr), _filterContext(nullptr),
      _mpBuffer(nullptr), _mpExpectedSize(0), _mpReceivedSize(0), 
      _mpSenderId(0), _mpType(0), _mpLastChunkTime(0) {
}

// Asynchronous Transmission Queue for Multipart messages
static struct {
#ifdef DEVICE_ROLE_HUB
    uint8_t buffer[512];
#else
    uint8_t buffer[192];
#endif
    size_t length;
    uint8_t targetId;
    uint16_t type;
    uint16_t segment;
    uint16_t totalSegments;
    unsigned long lastTime;
    bool active;
} asyncTx;

bool TransportService::isBusy() const {
    return asyncTx.active;
}

void TransportService::begin(uint8_t nodeId) {
    _nodeId = nodeId;
    _radio.begin(nodeId); // Re-init radio with correct address/pipes
}

void TransportService::loop() {
    // Multipart Timeout (2 seconds)
    if (_mpBuffer && (millis() - _mpLastChunkTime > 2000)) {
        iceLog.println("Multipart transfer timed out. Discarding buffer.");
        free(_mpBuffer);
        _mpBuffer = nullptr;
        _mpExpectedSize = 0;
        _mpReceivedSize = 0;
    }

    // Async TX Queue Processing (40ms non-blocking interval to prevent brownouts)
    if (asyncTx.active && (millis() - asyncTx.lastTime >= 40)) {
        if (asyncTx.segment == 0) {
            IcePacket header;
            memset(&header, 0, sizeof(header));
            header.targetID = asyncTx.targetId;
            header.senderID = _nodeId;
            header.msgType = PACKET_MULTIPART;
            header.payload.multipart.segmentId = 0;
            header.payload.multipart.header.totalSize = (uint16_t)asyncTx.length;
            header.payload.multipart.header.type = asyncTx.type;
            
            _radio.send(asyncTx.targetId, header);
            asyncTx.segment++;
        } else if (asyncTx.segment <= asyncTx.totalSegments) {
            IcePacket chunk;
            memset(&chunk, 0, sizeof(chunk));
            chunk.targetID = asyncTx.targetId;
            chunk.senderID = _nodeId;
            chunk.msgType = PACKET_MULTIPART;
            chunk.payload.multipart.segmentId = asyncTx.segment;
            
            size_t offset = (asyncTx.segment - 1) * 27;
            size_t chunkLen = asyncTx.length - offset;
            if (chunkLen > 27) chunkLen = 27;
            
            memcpy(chunk.payload.multipart.data, asyncTx.buffer + offset, chunkLen);
            
            _radio.send(asyncTx.targetId, chunk);
            asyncTx.segment++;
        } else {
            asyncTx.active = false; // Finished sending all chunks
        }
        asyncTx.lastTime = millis();
    }
}

bool TransportService::sendMessage(uint8_t targetId, uint16_t type, const void* data, size_t length) {
    // 1. Local Loopback
    if (targetId == _nodeId) {
        dispatch(_nodeId, targetId, type, (const uint8_t*)data, length);
        return true;
    }

    // 2. Broadcast (Smart or Dumb)
    if (targetId == 255) {
        // If we have a filter, use it to send unicasts to interested nodes
        // Exception: Registration ACKs must be a "dumb" broadcast for new nodes to hear.
        if (_broadcastFilter && type != PACKET_REGISTER_ACK) {
            bool sentAny = false;
            // Iterate through valid node IDs (1-254)
            for (uint8_t i = 1; i < 255; i++) {
                if (i == _nodeId) continue; 
                
                if (_broadcastFilter(i, type, _filterContext)) {
                    // Recursively call sendMessage to handle fragmentation/optimization for each target
                    if (sendMessage(i, type, data, length)) {
                        sentAny = true;
                    }
                }
            }
            return sentAny;
        } else {
            // Dumb broadcast (Target 255)
            return sendFragmented(255, type, (const uint8_t*)data, length);
        }
    }

    // 3. Unicast Remote
    // Optimization: If message fits in a standard packet, send it directly.
    // Payload capacity is 32 (Max) - 3 (Header) = 29 bytes.
    if (length <= 29 && type < 255) {
        IcePacket packet;
        memset(&packet, 0, sizeof(packet));
        packet.targetID = targetId;
        packet.senderID = _nodeId;
        packet.msgType = (uint8_t)type;
        memcpy(&packet.payload, data, length);
        return _radio.send(targetId, packet);
    }

    return sendFragmented(targetId, type, (const uint8_t*)data, length);
}

bool TransportService::sendFragmented(uint8_t targetId, uint16_t type, const uint8_t* data, size_t length) {
    if (asyncTx.active) return false; // Safety: Drop if already transmitting a multi-part payload
    if (length > sizeof(asyncTx.buffer)) return false; // Safety: Exceeds 256 bytes

    // Queue the data and let the loop() handle it non-blockingly
    memcpy(asyncTx.buffer, data, length);
    asyncTx.length = length;
    asyncTx.targetId = targetId;
    asyncTx.type = type;
    asyncTx.segment = 0;
    asyncTx.totalSegments = (length + 26) / 27;
    asyncTx.lastTime = millis();
    asyncTx.active = true;

    return true;
}

void TransportService::onMessageReceived(MessageCallback callback, void* context) {
    _callback = callback;
    _callbackContext = context;
}

void TransportService::setBroadcastFilter(BroadcastFilter filter, void* context) {
    _broadcastFilter = filter;
    _filterContext = context;
}

void TransportService::processPacket(const IcePacket& packet) {
    // Handle Multipart
    if (packet.msgType == PACKET_MULTIPART) {
        handleMultipartPacket(packet);
        return;
    }

    // Handle Standard Packets (Wrap payload in Message)
    const uint8_t* payloadData = (const uint8_t*)&packet.payload;
    dispatch(packet.senderID, packet.targetID, packet.msgType, payloadData, sizeof(packet.payload));
}

void TransportService::handleMultipartPacket(const IcePacket& packet) {
    uint16_t segmentId = packet.payload.multipart.segmentId;

    if (segmentId == 0) {
        // --- Header Packet ---
        uint16_t totalSize = packet.payload.multipart.header.totalSize;
        uint16_t type = packet.payload.multipart.header.type;
        
        if (_mpBuffer) { free(_mpBuffer); _mpBuffer = nullptr; }
        if (totalSize > 4096) return; // Safety limit

        _mpBuffer = (uint8_t*)malloc(totalSize + 1);
        if (!_mpBuffer) return;
        
        _mpExpectedSize = totalSize;
        _mpReceivedSize = 0;
        _mpSenderId = packet.senderID;
        _mpType = type;
        _mpLastChunkTime = millis();
        
    } else {
        // --- Data Chunk ---
        if (!_mpBuffer || packet.senderID != _mpSenderId) return;
        
        size_t offset = (segmentId - 1) * 27;
        if (offset >= _mpExpectedSize) return;
        
        size_t chunkSize = _mpExpectedSize - offset;
        if (chunkSize > 27) chunkSize = 27;
        
        memcpy(_mpBuffer + offset, packet.payload.multipart.data, chunkSize);
        _mpReceivedSize += chunkSize;
        _mpLastChunkTime = millis();
        
        if (_mpReceivedSize >= _mpExpectedSize) {
            _mpBuffer[_mpExpectedSize] = 0; // Null terminate
            dispatch(_mpSenderId, _nodeId, _mpType, _mpBuffer, _mpExpectedSize);
            
            free(_mpBuffer);
            _mpBuffer = nullptr;
            _mpExpectedSize = 0;
            _mpReceivedSize = 0;
        }
    }
}

void TransportService::dispatch(uint8_t source, uint8_t target, uint16_t type, const uint8_t* data, size_t length) {
    if (_callback) {
        Message msg;
        msg.sourceId = source;
        msg.targetId = target;
        msg.type = type;
        msg.data = data;
        msg.length = length;
        _callback(msg, _callbackContext);
    }
}