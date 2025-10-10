# Heltec LoRa 32 V3 Implementation Guide

This guide provides detailed information for using LoRaMesher with the Heltec LoRa 32 V3 board.

## Table of Contents
- [Hardware Overview](#hardware-overview)
- [Pin Configuration](#pin-configuration)
- [Quick Start](#quick-start)
- [Examples](#examples)
- [Configuration Options](#configuration-options)
- [Troubleshooting](#troubleshooting)

## Hardware Overview

The **Heltec WiFi LoRa 32 V3** is an advanced development board featuring:

- **MCU**: ESP32-S3FN8 (Dual-core Xtensa LX7, up to 240MHz)
- **LoRa Module**: Semtech SX1262
- **Frequency**: 863-928 MHz (depending on region/version)
- **Display**: 0.96" OLED (128x64 pixels, SSD1306 driver)
- **Connectivity**: WiFi 802.11 b/g/n, Bluetooth 5.0 LE
- **Memory**: 8MB Flash, 512KB SRAM
- **Power**: USB-C, LiPo battery support with charging circuit

## Pin Configuration

### LoRa Module (SX1262) Pins

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| CS/NSS   | 8        | SPI Chip Select |
| DIO1     | 14       | LoRa IRQ pin |
| RST      | 12       | Reset pin |
| BUSY     | 13       | Busy indicator |
| MOSI     | 10       | SPI MOSI |
| MISO     | 11       | SPI MISO |
| SCK      | 9        | SPI Clock |

### OLED Display (I2C) Pins

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| SDA      | 17       | I2C Data |
| SCL      | 18       | I2C Clock |
| RST      | 21       | Display Reset |

### Other Pins

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| LED      | 35       | Built-in LED |
| VEXT     | 36       | External power control |
| ADC      | 1        | Battery voltage monitoring |

## Quick Start

### 1. Hardware Setup

1. Connect your Heltec LoRa 32 V3 to your computer via USB-C
2. Ensure you have the correct antenna connected to the LoRa module
3. **Important**: Never power on the board without an antenna - this can damage the RF amplifier!

### 2. Software Setup

#### Install PlatformIO

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install the [PlatformIO extension](https://platformio.org/install/ide?install=vscode)

#### Open Example Project

1. Open the `examples/HeltecV3` or `examples/HeltecV3-Display` folder in VS Code
2. PlatformIO will automatically detect the project
3. Build and upload to your board

### 3. Basic Configuration

The minimal configuration for Heltec V3:

```cpp
#include <Arduino.h>
#include "LoraMesher.h"

// Pin definitions
#define LORA_CS     8
#define LORA_IRQ    14
#define LORA_RST    12
#define LORA_IO1    14

// Custom SPI pins
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9

LoraMesher& radio = LoraMesher::getInstance();
SPIClass customSPI(HSPI);

void setup() {
    Serial.begin(115200);
    
    // Initialize SPI
    customSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    
    // Configure LoRaMesher
    LoraMesher::LoraMesherConfig config;
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    config.loraCs = LORA_CS;
    config.loraIrq = LORA_IRQ;
    config.loraRst = LORA_RST;
    config.loraIo1 = LORA_IO1;
    config.spi = &customSPI;
    config.freq = 868.0; // Adjust for your region
    
    radio.begin(config);
    radio.start();
}
```

## Examples

### 1. Basic Counter (`examples/HeltecV3`)

A simple example that:
- Broadcasts a counter packet every 20 seconds
- Receives and displays packets from other nodes
- Blinks LED on transmit/receive

**Use case**: Testing basic mesh functionality, range testing

### 2. Counter with Display (`examples/HeltecV3-Display`)

An enhanced example featuring:
- OLED display showing node status
- Real-time packet information
- Routing table size display
- Visual feedback for all operations

**Use case**: Network monitoring, demonstration, development

## Configuration Options

### Frequency Bands

Set the appropriate frequency for your region:

```cpp
config.freq = 868.0;  // Europe (863-870 MHz)
// config.freq = 915.0;  // North America (902-928 MHz)
// config.freq = 433.0;  // Asia (433 MHz)
```

### LoRa Parameters

Adjust these based on your requirements:

```cpp
config.bw = 125.0;              // Bandwidth in kHz (7.8 to 500)
config.sf = 7;                  // Spreading Factor (6-12)
config.cr = 7;                  // Coding Rate (5-8)
config.power = 14;              // TX Power in dBm (2-22 for SX1262)
config.syncWord = 0x12;         // Network identifier
config.preambleLength = 8;      // Preamble length
```

**Note**: Different SF values affect range vs data rate:
- SF7: Fastest, shortest range (~2km)
- SF12: Slowest, longest range (~15km)

### Display Configuration

If using the OLED display:

```cpp
#include "display.h"

Display Screen;

void setup() {
    Screen.initDisplay();
    Screen.changeLineOne("Hello World");
    Screen.drawDisplay();
}
```

## Troubleshooting

### Common Issues

#### 1. Board Not Detected

**Problem**: Computer doesn't recognize the board

**Solutions**:
- Install [CP210x USB drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- Try a different USB cable (must be data cable, not charge-only)
- Press and hold the "PRG" button while connecting USB

#### 2. Upload Fails

**Problem**: Upload error or timeout

**Solutions**:
- Hold "PRG" button during upload
- Set upload speed to 115200 in platformio.ini
- Check serial port permissions (Linux/macOS)

#### 3. No LoRa Communication

**Problem**: Not sending/receiving packets

**Solutions**:
- Verify antenna is connected
- Check frequency matches your region and hardware
- Ensure all nodes use the same LoRa parameters
- Verify SPI pins are correctly configured

#### 4. Display Not Working

**Problem**: OLED screen stays blank

**Solutions**:
- Verify I2C address (0x3C or 0x3D)
- Check SDA/SCL pin definitions (17, 18)
- Add pull-up resistors if needed (usually built-in)
- Verify Wire.begin() with correct pins

#### 5. High Power Consumption

**Problem**: Battery drains quickly

**Solutions**:
- Control VEXT pin (GPIO 36) to power off display when not needed
- Use sleep modes between transmissions
- Reduce TX power if range permits
- Optimize transmission intervals

### Debug Tips

Enable verbose logging:

```cpp
// In setup()
Serial.setDebugOutput(true);
```

Monitor RSSI and SNR:

```cpp
float rssi = radio.getRSSI();
float snr = radio.getSNR();
Serial.printf("RSSI: %.2f dBm, SNR: %.2f dB\n", rssi, snr);
```

Check free heap memory:

```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
```

## Performance Characteristics

### Range

Expected range with SF7, 125kHz BW, 14dBm power:
- Urban: 1-3 km
- Suburban: 3-5 km
- Rural/Line of sight: 5-15 km

### Data Rate

Approximate throughput with SF7, 125kHz BW:
- Physical: ~5.5 kbps
- Effective (with overhead): ~3-4 kbps

### Power Consumption

Typical current draw:
- Active RX: ~13 mA
- Active TX (14dBm): ~120 mA
- Light sleep: ~0.8 mA
- Deep sleep: ~10 µA

## Additional Resources

### Official Documentation
- [Heltec V3 Documentation](https://heltec.org/project/wifi-lora-32-v3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)

### Community
- [LoRaMesher GitHub](https://github.com/LoRaMesher/LoRaMesher)
- [LoRaMesher Discord](https://discord.gg/SSaZhsChjQ)
- [Heltec Forum](https://heltec.org/community/)

### Tools
- [LoRa Calculator](https://www.loratools.nl/#/airtime) - Calculate airtime and range
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/) - Underlying radio library

## License

This implementation follows the same license as the main LoRaMesher project.

## Contributing

Contributions are welcome! Please:
1. Test thoroughly on actual hardware
2. Document any new features
3. Follow the existing code style
4. Submit pull requests to the main repository

---

**Last Updated**: October 2025  
**Branch**: Heltec-V3
