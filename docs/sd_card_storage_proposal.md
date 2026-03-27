# SD Card Storage & Log Rotation Proposal

## Overview
Adding an SD Card to the IceHub (ESP32) elevates it to a true micro-server. It provides two major benefits:
1. **Static Web Hosting:** Decouples the UI from the C++ code, allowing rich HTML, CSS, and JS files to be served directly from the SD card.
2. **Persistent Logging:** Stores `IceHubLog` data long-term for offline troubleshooting, surviving reboots and crashes.

## 1. Hardware Requirements & Wiring
MicroSD cards communicate natively over SPI. Because the IceHub already uses SPI for the NRF24L01 radio, the SD card will simply share the bus.

* **MOSI, MISO, SCK:** Wire these directly to the exact same pins used by the NRF24L01.
* **CS (Chip Select):** Requires **one** new dedicated GPIO pin. The ESP32 pulls this pin low when it wants to talk to the SD card instead of the radio.
* **Power:** Ensure the SD card module receives 3.3V (most breakout boards have an onboard 5V-to-3.3V regulator).

## 2. Software Architecture: `SdStorageService`
To implement this cleanly without blocking the Hub's radio loop, we will introduce a new `SdStorageService` that extends the `IceService` interface and runs in the main `ServiceChain`.

### A. Static Web Serving
The ESP32 `WebServer` library has native SD card support. Once `SD.begin(SD_CS_PIN)` is called, the `UiHandler` can be updated to seamlessly route web requests to the SD card:

```cpp
// Inside UiHandler (or a new SdCardUiHandler)
_server->serveStatic("/", SD, "/www/");
```
This automatically maps `http://hub-ip/` to `/www/index.html` on the SD card.

### B. Buffered Log Rotation
Writing directly to an SD card for every single log line is dangerous:
1. **Flash Wear:** SD cards write in 4KB blocks. Writing 64 bytes forces a read-modify-erase-write cycle of the entire block, destroying the card quickly.
2. **Blocking I/O:** File writes take time and could cause the hub to drop incoming radio packets.
3. **Infinite Growth:** An endlessly growing log file will eventually fill the card and crash the filesystem.

**The Solution:**
The `SdStorageService` will monitor `IceHubLog` and write logs out using a **Buffered Rolling File** strategy:

1. **Buffering:** Instead of writing every line, the service will periodically pull the oldest entries from the dynamic array and write them in large chunks (e.g., 4KB at a time, or once every few minutes).
2. **Rolling Logs:** 
    * All new logs are appended to `/logs/current.txt`.
    * Before writing a chunk, the service checks the file size.
    * If `current.txt` exceeds 2 Megabytes, the service:
        1. Deletes `/logs/archive.txt` (if it exists).
        2. Renames `/logs/current.txt` to `/logs/archive.txt`.
        3. Creates a new `/logs/current.txt`.

This guarantees that you always have up to 4MB of the most recent persistent logs, and the SD card is protected from excessive wear.

## 3. Configuration Management (Hybrid Approach)
It might be tempting to move the Hub's WiFi and MQTT configuration to a `config.json` file on the SD card. **This is not recommended.**

* **Critical Config (NVS):** Keep WiFi SSID/Password, Node IDs, and Security Tokens in the ESP32's internal Non-Volatile Storage (NVS). If the SD card pops out or gets corrupted, the Hub will still boot, connect to WiFi, and manage the lights securely.
* **Extended Config (SD Card):** Use the SD card for non-critical user preferences, such as custom dashboard layouts, color palettes, or future scheduling rules (e.g., "Turn on porch at 8 PM").

## Implementation Steps
1. Wire up the SD Card breakout board.
2. Add `#define SD_CS_PIN X` to your `ice_protocol.h` or config.
3. Create `SdStorageService.h / .cpp`.
4. Add `sdStorageService` to `main.cpp` and register it in `services.add()`.
5. Update `UiHandler` to try `serveStatic` before falling back to the hardcoded HTML string.