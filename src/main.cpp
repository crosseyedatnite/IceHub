#include <Arduino.h>
#include "ice_protocol.h"
#include "ice_radio.h"
#include "service_chain.h"
#include "transport_service.h"
#include "effect_controller.h"
#include "ice_effects.h" // Needed for discovery list

#include "IceHubLog.h"
IceHubLog iceLog;

#ifdef HAS_Si7021_SENSOR
#include "TempHumiditySensor.h"
TempHumiditySensor tempSensor(iceLog);
#endif

#ifdef HAS_HLK_LK2420_SENSOR
#include "RadarPresenceSensor.h"
RadarPresenceSensor radarSensor(iceLog);
#endif

IceRadio iceRadio(RADIO_CE_PIN, RADIO_CSN_PIN);

#ifdef HAS_LEDS
#include <FastLED.h>
#include "ice_effects.h"
#ifndef LED_PIN
#define LED_PIN 6
#endif
#ifndef NUM_LEDS
#define NUM_LEDS 16
#endif
CRGB leds[NUM_LEDS];
IceEffects effects(leds, NUM_LEDS);
#endif

// Transport Service is used by both Hub and Remote
TransportService transport(iceRadio);

#ifdef HAS_LEDS
EffectController effectController(transport, effects);
#else
EffectController effectController(transport);
#endif

#ifdef DEVICE_ROLE_HUB
// --- HUB INCLUDES ---
#include <WiFi.h>
#include <PubSubClient.h>

#include "ice_hub.h"
#include "mqtt_transport.h"
#include "home_assistant_service.h"
#include "system_config.h"

#include "node_registry.h"
#include "capability_manager.h"
#include "web_adapter.h"


SystemConfig systemConfig;
NodeRegistry registry;
CapabilityManager caps;
MqttTransport mqtt;
WebAdapter webAdapter(registry, caps, effectController);
HomeAssistantService haService(mqtt, registry, caps, effectController);
IceHub iceHub(systemConfig, transport, registry, caps, webAdapter, effectController);
#endif

#ifdef DEVICE_ROLE_REMOTE
// --- REMOTE INCLUDES ---
#include "remote_node.h"
#ifdef HAS_LEDS
RemoteNode remoteNode(transport, effectController, effects);
#else
RemoteNode remoteNode(transport);
#endif
#endif

ServiceChain services;

void setup() {
    Serial.begin(SERIAL_BAUD);
    iceLog.println(F("Booting IceHub System..."));

    iceLog.printf(F("Radio Pins: CE=%d CSN=%d"), RADIO_CE_PIN, RADIO_CSN_PIN);
    #ifdef RADIO_SCK_PIN
    iceLog.printf(F(" SCK=%d MOSI=%d MISO=%d\n"), RADIO_SCK_PIN, RADIO_MOSI_PIN, RADIO_MISO_PIN);
    #else
    iceLog.println("");
    #endif

    iceLog.println(F("Setting up services..."));

    iceLog.setup();
    services.add(iceLog);

    #ifdef HAS_Si7021_SENSOR
    tempSensor.setup();
    tempSensor.onSensorEvent([](float temp, float hum, void* ctx) {
        SensorPayload payload;
        memset(&payload, 0, sizeof(payload));
        payload.temperature = (int16_t)(temp * 100.0f);
        payload.humidity = (uint16_t)(hum * 100.0f);
        transport.sendMessage(0, PACKET_SENSOR, &payload, sizeof(payload));
    }, nullptr);
    services.add(tempSensor);
    #endif

    #ifdef HAS_HLK_LK2420_SENSOR
    radarSensor.setup();
    // For now, the radar just logs presence changes locally via iceLog
    services.add(radarSensor);
    #endif

    iceLog.println(F("Services initialized. Defining callbacks for transport"));
    // Pass raw radio packets to Transport Service
    iceRadio.onPacketReceived([](const IcePacket& packet) {
        transport.processPacket(packet);
    });

    #ifdef DEVICE_ROLE_HUB
    iceLog.println(F("Role: HUB (Gateway)"));
    
    mqtt.begin();
    haService.begin();
    iceHub.begin();
    #endif

    #ifdef DEVICE_ROLE_REMOTE
    iceLog.println(F("Role: REMOTE (Node)"));
    remoteNode.begin();
    #endif

    // Configure Chain of Responsibility
    services.add(iceRadio);
    services.add(transport);

    #ifdef DEVICE_ROLE_HUB
    services.add(mqtt);
    services.add(haService);
    services.add(iceHub);
    #endif

    #ifdef DEVICE_ROLE_REMOTE
    services.add(remoteNode);
    #endif

    #ifdef HAS_LEDS
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    effects.begin();
    services.add(effects);
    #endif
}

void loop() {
    services.loop();
}