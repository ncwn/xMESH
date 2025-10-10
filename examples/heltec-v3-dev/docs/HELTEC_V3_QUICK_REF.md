# Heltec LoRa 32 V3 - Quick Reference Card

## Hardware Quick Facts
- **MCU**: ESP32-S3FN8 (Dual-core Xtensa LX7 @ 240MHz)
- **LoRa**: SX1262 (863-928 MHz)
- **Flash**: 8MB | **RAM**: 512KB
- **Display**: 0.96" OLED (128x64, I2C)
- **Connectivity**: WiFi + BLE 5.0

## Pin Quick Reference

```cpp
// LoRa SX1262
#define LORA_CS     8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9

// Display (I2C)
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21

// Other
#define LED         35
#define VEXT        36  // Power control
#define ADC_VBAT    1   // Battery voltage
```

## Minimal Setup Code

```cpp
#include "LoraMesher.h"

SPIClass customSPI(HSPI);
LoraMesher& radio = LoraMesher::getInstance();

void setup() {
    Serial.begin(115200);
    customSPI.begin(9, 11, 10, 8); // SCK, MISO, MOSI, CS
    
    LoraMesher::LoraMesherConfig config;
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    config.loraCs = 8;
    config.loraIrq = 14;
    config.loraRst = 12;
    config.loraIo1 = 14;
    config.spi = &customSPI;
    config.freq = 868.0; // YOUR_REGION_FREQUENCY
    
    radio.begin(config);
    radio.start();
}
```

## Regional Frequencies

| Region | Frequency | Legal Bands |
|--------|-----------|-------------|
| EU     | 868.0 MHz | 863-870 MHz |
| US     | 915.0 MHz | 902-928 MHz |
| Asia   | 433.0 MHz | 433.0-435.0 MHz |
| AU     | 915.0 MHz | 915-928 MHz |

## LoRa Parameter Guide

### Spreading Factor vs Range/Speed
| SF  | Speed | Range | Use Case |
|-----|-------|-------|----------|
| 7   | Fast  | 2km   | High data rate |
| 9   | Med   | 5km   | Balanced |
| 12  | Slow  | 15km  | Maximum range |

### Bandwidth Options
| BW (kHz) | Use Case |
|----------|----------|
| 125      | Standard (recommended) |
| 250      | Higher throughput |
| 500      | Maximum speed |

### TX Power (SX1262)
| Power | Range | Current |
|-------|-------|---------|
| 10 dBm | Short | ~80 mA |
| 14 dBm | Medium | ~120 mA |
| 22 dBm | Max | ~140 mA |

## Display Code Snippets

```cpp
#include "display.h"
Display Screen;

// Initialize
Screen.initDisplay();

// Update lines
Screen.changeLineOne("Line 1");
Screen.changeLineTwo("Line 2");
Screen.changeLineThree("Line 3");
Screen.changeLineFour("Line 4");

// Refresh display
Screen.drawDisplay();
```

## Common Operations

### Send Broadcast Packet
```cpp
struct dataPacket { uint32_t value; };
dataPacket* packet = new dataPacket;
packet->value = 123;
radio.createPacketAndSend(BROADCAST_ADDR, packet, 1);
```

### Receive Packets
```cpp
void processReceivedPackets(void*) {
    for (;;) {
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        while (radio.getReceivedQueueSize() > 0) {
            AppPacket<dataPacket>* packet = 
                radio.getNextAppPacket<dataPacket>();
            // Process packet...
            radio.deletePacket(packet);
        }
    }
}
```

### Get Network Info
```cpp
uint16_t localAddr = radio.getLocalAddress();
size_t routeCount = radio.routingTableSize();
```

## Troubleshooting Quick Fixes

| Problem | Solution |
|---------|----------|
| Upload fails | Hold PRG button |
| No detection | Install CP210x driver |
| No LoRa | Check antenna & frequency |
| Display blank | Verify I2C pins (17, 18) |
| High power use | Control VEXT (GPIO 36) |

## Important Notes

⚠️ **CRITICAL**: Always connect antenna before powering on!  
⚠️ Match LoRa parameters across all nodes  
⚠️ Respect regional frequency regulations  
⚠️ Use appropriate TX power for your needs  

## Resources

- Examples: `examples/HeltecV3/` and `examples/HeltecV3-Display/`
- Full Guide: `HELTEC_V3_GUIDE.md`
- Discord: https://discord.gg/SSaZhsChjQ

## platformio.ini Template

```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
framework = arduino
monitor_speed = 115200
build_flags = 
    -DARDUINO_HELTEC_WIFI_LORA_32_V3
    -DHELTEC_V3
lib_deps = 
    jgromes/RadioLib@^6.6.0
```

---
**Version**: 1.0 | **Branch**: Heltec-V3 | **Date**: October 2025
