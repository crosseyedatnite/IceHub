#include "ice_radio.h"
#include <SPI.h>

#include "IceHubLog.h"
extern IceHubLog iceLog;

#ifndef RADIO_CHANNEL
#define RADIO_CHANNEL 100
#endif

// Base address for the network. 
// Hub listens on this address. Nodes write to this address.
const uint64_t HUB_ADDRESS      = 0xE8E8F0F0E1LL;
const uint64_t NODE_ADDRESS_BASE = 0xE8E8F0F000LL;

#ifdef USE_RADIO_IRQ
// 1. The flag must be volatile so the compiler knows it can change at any time
volatile bool radio_interrupt_fired = false;

// 2. The Interrupt Service Routine (ISR) triggered by the hardware pin.
// We CANNOT use SPI (radio.read) inside here. We only set a flag.
#if defined(ESP32) || defined(ESP8266)
void IRAM_ATTR radioISR() {
#else
void radioISR() {
#endif
    radio_interrupt_fired = true;
}
#endif

// Track initialization state to prevent browning out if hardware fails
static bool _radio_initialized = false;

IceRadio::IceRadio(uint8_t ce_pin, uint8_t csn_pin) : _radio(ce_pin, csn_pin), _nodeId(0) {}

bool IceRadio::begin(uint8_t nodeId) {
    _nodeId = nodeId;
    _radio_initialized = false;
    
    #ifdef ESP32
    // Explicitly initialize SPI with the user's pin mapping
    SPI.begin(RADIO_SCK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CSN_PIN);
    #endif

    delay(50); // Allow NRF24 hardware to stabilize power before initializing

    if (!_radio.begin()) {
        iceLog.println(F("Radio hardware not responding!"));
        return false;
    }

    iceLog.println(F("Configuring radio settings..."));
    iceLog.printf(F("Channel: %d\n"), RADIO_CHANNEL);
    iceLog.println(F("Data Rate: 1Mbps"));
    iceLog.println(F("PALevel: LOW"));

    _radio.setPALevel(RF24_PA_LOW); // Low power to reduce interference/heat
    _radio.setDataRate(RF24_1MBPS);
    _radio.setChannel(RADIO_CHANNEL);
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

#ifdef USE_RADIO_IRQ
    pinMode(RADIO_IRQ_PIN, INPUT_PULLUP);
    // Note: NRF24 natively pulls IRQ LOW on events, so we trigger on FALLING
    attachInterrupt(digitalPinToInterrupt(RADIO_IRQ_PIN), radioISR, FALLING);
    // Tell the radio to ONLY pull the IRQ pin low for RX (data received) events
    _radio.maskIRQ(1, 1, 0); 
#endif

    iceLog.printf(F("Radio initialized. Node ID: %d\n"), _nodeId);

    #ifdef RADIO_DEBUG
    _radio.printDetails();
    #endif
        
    _radio_initialized = true;
    return true;
}

void IceRadio::loop() {
    if (!_radio_initialized) return;

#ifdef USE_RADIO_IRQ
    // If the interrupt hasn't fired, we skip the slow SPI bus check entirely!
    if (!radio_interrupt_fired) return;
    
    // Acknowledge the interrupt
    radio_interrupt_fired = false;
    _radio.clearStatusFlags(); // Clear the NRF24 hardware interrupt register
#endif

    // Process all available packets in the queue
    // (Changed from 'if' to 'while' to drain the queue when using interrupts)
    while (_radio.available()) {
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
            iceLog.printf(F("\r[Radio] Noise/Invalid Packet (len=%d, Expected=%d)   \n"), len, sizeof(IcePacket));
        }
    } 
}

bool IceRadio::send(uint8_t targetId, const IcePacket& packet) {
    if (!_radio_initialized) return false;

    _radio.stopListening();
    uint64_t addr = (targetId == 0) ? HUB_ADDRESS : (NODE_ADDRESS_BASE + targetId);
    _radio.openWritingPipe(addr);
    bool result = _radio.write(&packet, sizeof(IcePacket));
    _radio.startListening();
    return result;
}

void IceRadio::onPacketReceived(RadioCallback callback) {
    _callback = callback;
}