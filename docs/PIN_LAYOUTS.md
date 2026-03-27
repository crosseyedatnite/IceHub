# Hardware Pin Configuration

This document summarizes the default pin assignments for the various hardware environments defined in `platformio.ini`.

These defaults can be overridden by adding flags to `build_flags` in `platformio.ini` (e.g., `-D CE_PIN=4`).

## 1. Standard Arduino Nano (External Radio)
**Environments:** `env:nanoatmega328`, `env:nano_debug`
**Hardware:** Generic Nano V3 + NRF24L01+ Module

| Component | Pin | Notes |
| :--- | :--- | :--- |
| **Radio CE** | **7** | |
| **Radio CSN** | **8** | |
| **Radio MOSI** | **11** | Hardware SPI |
| **Radio MISO** | **12** | Hardware SPI |
| **Radio SCK** | **13** | Hardware SPI |
| **LED Data** | **6** | WS2812B Strip |
| **Button** | **3** | Active LOW (Connect to GND) |

## 2. RF-Nano (Internal Radio)
**Environments:** `env:rf_nanoatmega328`
**Hardware:** Emakefun/Keywish RF-Nano (Nano with onboard NRF24)

| Component | Pin | Notes |
| :--- | :--- | :--- |
| **Radio CE** | **10** | Hardwired internally |
| **Radio CSN** | **9** | Hardwired internally |
| **Radio MOSI** | **11** | Hardware SPI |
| **Radio MISO** | **12** | Hardware SPI |
| **Radio SCK** | **13** | Hardware SPI |
| **LED Data** | **6** | |
| **Button** | **3** | |

## 3. ESP32 (Sender)
**Environments:** `env:esp32_sender`
**Hardware:** ESP32 Dev Module + NRF24L01+ Adapter

| Component | Pin | Notes |
| :--- | :--- | :--- |
| **Radio CE** | **12** | Configurable |
| **Radio CSN** | **5** | Configurable |
| **Radio MOSI** | **23** | VSPI Default |
| **Radio MISO** | **19** | VSPI Default |
| **Radio SCK** | **18** | VSPI Default |
| **LED Data** | **6** | |
| **Button** | **27** | Active LOW |

## 4. Nano - New Prototype Board
| Component | Pin | Notes |
| :--- | :--- | :--- |
| **Radio CE** | **9** | Hardwired internally |
| **Radio CSN** | **10** | Hardwired internally |
| **Radio MOSI** | **11** | Hardware SPI |
| **Radio MISO** | **12** | Hardware SPI |
| **Radio SCK** | **13** | Hardware SPI |
| **Radio IRQ** | **2** | Active HIGH |

