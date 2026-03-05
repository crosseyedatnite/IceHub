#include <Arduino.h>
#include "ice_protocol.h"
#include "ice_radio.h"

IceRadio radio(RADIO_CE_PIN, RADIO_CSN_PIN);

#ifdef DEVICE_ROLE_HUB
// --- HUB INCLUDES ---
#include <WiFi.h>
#include <PubSubClient.h>
#include "ice_effects.h" // Needed for discovery list
#include "hub_network.h"
HubNetwork hubNetwork;
#endif

#ifdef DEVICE_ROLE_REMOTE
// --- REMOTE INCLUDES ---
#include <EEPROM.h>
#ifdef HAS_LEDS
#include <FastLED.h>
#include "ice_effects.h"
#endif
#endif

#ifdef DEVICE_ROLE_REMOTE
#define LED_PIN 6
#ifndef NUM_LEDS
#define NUM_LEDS 16
#endif
CRGB leds[NUM_LEDS];
IceEffects effects(leds, NUM_LEDS);
uint8_t myNodeId = 255;
unsigned long lastRegAttempt = 0;
unsigned long regAttemptCount = 0;


void handleRemotePacket(const IcePacket& packet) {
    // Filter packets not for us (Broadcast is 255)
    if (packet.targetID != myNodeId && packet.targetID != 255) return;

    if (packet.msgType == PACKET_STATE) {
        // Update Effects Engine
        if (packet.payload.effect.mode != 255) {
            DisplayMode mode = static_cast<DisplayMode>(packet.payload.effect.mode);
            if (mode == MANUAL_SOLID) {
                effects.setManualColor(packet.payload.effect.r, packet.payload.effect.g, packet.payload.effect.b);
            }
            effects.setMode(mode);
        }
        if (packet.payload.effect.brightness != 255) {
            effects.setBrightness(packet.payload.effect.brightness);
        }
    }
    else if (packet.msgType == PACKET_REGISTER_ACK) {
        // Registration Response
        if (myNodeId == 255 && packet.payload.registration.nonce == 0x1CE1) {
             uint8_t newId = packet.payload.registration.assignedId;
             Serial.print("Registered! Assigned ID: "); Serial.println(newId);
             EEPROM.write(0, newId);
             // Reset to apply new ID
             void (*resetFunc)(void) = 0;
             resetFunc();
        }
    }
    else if (packet.msgType == PACKET_CONFIG) {
        // Configuration Request
        if (packet.payload.config.command == 1) { // 1 = Get Capabilities
            char buf[512]; // Temp buffer for JSON
            effects.getCapabilitiesJSON(buf, sizeof(buf));
            // Send back as Multipart (Type 1 = JSON Caps)
            radio.sendMultipart(0, 1, buf, strlen(buf));
        }
    }
}
#endif

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println("Booting IceHub System...");

    #ifdef DEVICE_ROLE_HUB
    Serial.println("Role: HUB (Gateway)");
    Serial.print("Radio Pins: CE="); Serial.print(RADIO_CE_PIN);
    Serial.print(" CSN="); Serial.print(RADIO_CSN_PIN);
    Serial.print(" SCK="); Serial.print(RADIO_SCK_PIN);
    Serial.print(" MOSI="); Serial.print(RADIO_MOSI_PIN);
    Serial.print(" MISO="); Serial.println(RADIO_MISO_PIN);
    
    if (radio.begin(0)) {
        Serial.println("Radio initialized (Hub)");
    } else {
        Serial.println("Radio failed to initialize");
    }
    
    radio.onPacketReceived([](const IcePacket& packet) {
        hubNetwork.handleRadioPacket(packet);
    });
    
    hubNetwork.onRadioSend([](uint8_t target, const IcePacket& packet) {
        radio.send(target, packet);
    });
    
    hubNetwork.begin();
    #endif

    #ifdef DEVICE_ROLE_REMOTE
    Serial.println("Role: REMOTE (Node)");
    
    EEPROM.get(0, myNodeId);
    if (myNodeId == 0) myNodeId = 255; // Safety check

    if (radio.begin(myNodeId)) {
        Serial.println("Radio initialized (Remote)");
        // Send initial Hello if we are already registered
        if (myNodeId != 255) {
            IcePacket hello;
            memset(&hello, 0, sizeof(hello));
            hello.senderID = myNodeId;
            hello.targetID = 0;
            hello.msgType = PACKET_PING;
            radio.send(0, hello);
        }
    } else {
        Serial.println("Radio failed to initialize");
    }
    
    radio.onPacketReceived(handleRemotePacket);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    effects.begin();
    #endif
}

void loop() {
    radio.update();

    #ifdef DEVICE_ROLE_HUB
    hubNetwork.loop();
    #endif
    
    #ifdef DEVICE_ROLE_REMOTE
    effects.run();
    // Registration Loop
    if (myNodeId == 255) {
        if (millis() - lastRegAttempt > 5000) {
            bool sendResult=false;
            lastRegAttempt = millis();
            IcePacket reg;
            reg.senderID = 255;
            reg.targetID = 0; // To Hub
            reg.msgType = PACKET_REGISTER_REQ;
            reg.payload.registration.nonce = 0x1CE1; // Simple nonce
            reg.payload.registration.deviceType = 1; // LED
            sendResult = radio.send(0, reg);
            Serial.print("\rSent Registration Request...");
            Serial.print(sendResult ? "Success" : "Failed");
            Serial.print(" Attempt #"); Serial.print(++regAttemptCount);

        }
    } else {
        EVERY_N_SECONDS(15) {
            // Periodic Status Update (e.g. battery level)
            IcePacket status;
            status.senderID = myNodeId;
            status.targetID = 0; // To Hub
            status.msgType = PACKET_SENSOR;
            status.payload.sensor.temperature = 0; // Placeholder
            status.payload.sensor.humidity = 0;    // Placeholder
            status.payload.sensor.batteryMv = 0;   // Placeholder
            radio.send(0, status);
        }
    }
    #endif
}