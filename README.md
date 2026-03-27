# IceHub

IceHub is a PlatformIO-based Arduino/ESP32 project that provides a flexible home automation hub with both local and networked device support.

## Key features

- RF24-based mesh node communication (NRF24 module)
- MQTT transport support for integration with Home Assistant and other MQTT consumers
- Local web interface for monitoring and control
- Temperature and humidity sensor support (Si7021) and mmWave radar presence sensor support (LD2420)
- Modular service architecture (`IceService` implementations)
- Remote logging and operations via radio and web

## Home Assistant integration

The `hub_esp32` environment is designed to bridge local RF24 nodes and MQTT entities to Home Assistant. In your setup, your Home Assistant server runs on a Raspberry Pi 5; for others, replace with your own broker host (e.g., `hassio.local`, `192.168.x.x`).

- Hub publishes MQTT topics for sensor readings and presence state
- Hub subscribes to command/control topics for effect and lighting actions
- Configure HA MQTT integration with the same broker settings used in `src/mqtt_transport.cpp` and `system_config` (if present)

## Repository structure

- `src/` - main firmware source code
- `include/` - include files and headers (project docs)
- `docs/` - design docs, pinouts, and proposals
- `platformio.ini` - build configuration for platform and frameworks

## Getting started

1. Open project in VS Code with PlatformIO.
2. Install required libraries via PlatformIO library manager.
3. Connect appropriate hardware (ESP32 as hub, NRF24 radio, sensors, etc.).
4. Build and upload using PlatformIO.

## PlatformIO environments and build flags

PlatformIO uses `platformio.ini` to define per-environment build targets, upload protocols, board definitions, and more.  In this project:

- `hub_esp32` is the ESP32 hub environment (primary coordinator, MQTT bridge, web interface, RF24 manager)
- `remote_nano` is an Arduino Nano remote node with Si7021 and LD2420 sensors (temperature/humidity + radar)
- `remote_rf_nano` is another Arduino Nano remote node for RF-only or alternate peripheral configurations

The `build_flags` section under each environment controls `#ifdef` behavior in source code:

- enables/disables sensor-specific code paths
- switches between hub and remote behaviors
- toggles debug or feature flags

### How to customize

1. Clone the repository:
   - `git clone <repo-url>`
   - `cd IceHub`
2. Open `platformio.ini` and edit or add environments for your hardware.
3. Set the appropriate `board` and `framework` values, and define `build_flags` as needed.
4. Build an environment:
   - `pio run -e hub_esp32`
   - `pio run -e remote_nano --target upload`

### Example snippet

```ini
[env:hub_esp32]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
    -D HUB_MODE
    -D MQTT_ENABLED

[env:remote_nano]
platform = atmelavr
board = nanoatmega328
framework = arduino
build_flags =
    -D REMOTE_MODE
    -D HAS_SI7021
    -D HAS_LD2420
```

- Add your own env like `env:my_custom_node` and set `build_flags` to match your sensors.
- Document pin mappings in `docs/PIN_LAYOUTS.md` and update your local wiring.

## Notes

- The project has targets for both hub and remote nodes under platformio environments.
- Use `docs/Implementation-Next-Steps.md` to track pending feature work (log fetching, sensor integration, etc.).
