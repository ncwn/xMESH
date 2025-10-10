# Migrating to Heltec LoRa 32 V3

This guide helps you migrate existing LoRaMesher projects to the Heltec LoRa 32 V3.

## Overview of Changes

The Heltec LoRa 32 V3 uses different hardware compared to common development boards:

| Feature | TTGO T-Beam | Heltec V3 | Notes |
|---------|-------------|-----------|-------|
| MCU | ESP32 | ESP32-S3 | More powerful |
| LoRa Chip | SX1276/1278 | SX1262 | Better performance |
| SPI Pins | Default | Custom | Requires configuration |
| Display | No | Yes | Built-in OLED |
| USB | Micro-USB | USB-C | Modern connector |

## Migration Steps

### 1. Update Board Definition

**Old (TTGO T-Beam):**
```ini
[env:ttgo-t-beam]
platform = espressif32
board = ttgo-t-beam
```

**New (Heltec V3):**
```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
build_flags = 
    -DARDUINO_HELTEC_WIFI_LORA_32_V3
    -DHELTEC_V3
```

### 2. Update Pin Definitions

**Old (TTGO T-Beam with SX1276):**
```cpp
// Default pins - no custom SPI needed
LoraMesher::LoraMesherConfig config;
config.module = LoraMesher::LoraModules::SX1276_MOD;
// Uses default SPI pins
```

**New (Heltec V3 with SX1262):**
```cpp
// Custom SPI instance required
SPIClass customSPI(HSPI);

void setup() {
    customSPI.begin(9, 11, 10, 8); // SCK, MISO, MOSI, CS
    
    LoraMesher::LoraMesherConfig config;
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    config.loraCs = 8;
    config.loraIrq = 14;
    config.loraRst = 12;
    config.loraIo1 = 14;
    config.spi = &customSPI;
    
    radio.begin(config);
}
```

### 3. Update LoRa Module Type

If migrating from SX127x series:

**Old:**
```cpp
config.module = LoraMesher::LoraModules::SX1276_MOD;
// or
config.module = LoraMesher::LoraModules::SX1278_MOD;
```

**New:**
```cpp
config.module = LoraMesher::LoraModules::SX1262_MOD;
```

### 4. Adjust LED Pin (if used)

**Old (T-Beam):**
```cpp
#define BOARD_LED 4
#define LED_ON LOW
#define LED_OFF HIGH
```

**New (Heltec V3):**
```cpp
#define BOARD_LED 35
#define LED_ON HIGH  // Note: inverted!
#define LED_OFF LOW
```

### 5. Add Display Support (Optional)

The Heltec V3 has a built-in OLED display. To use it:

**Add to platformio.ini:**
```ini
lib_deps = 
    jgromes/RadioLib@^6.6.0
    adafruit/Adafruit GFX Library@^1.11.9
    adafruit/Adafruit SSD1306@^2.5.9
```

**Add to your code:**
```cpp
#include "display.h"

Display Screen;

void setup() {
    Screen.initDisplay();
    Screen.changeLineOne("Hello World");
    Screen.drawDisplay();
}
```

## Complete Migration Example

### Before (TTGO T-Beam):

```cpp
#include <Arduino.h>
#include "LoraMesher.h"

#define BOARD_LED 4
#define LED_ON LOW
#define LED_OFF HIGH

LoraMesher& radio = LoraMesher::getInstance();

void setup() {
    Serial.begin(115200);
    pinMode(BOARD_LED, OUTPUT);
    
    LoraMesher::LoraMesherConfig config;
    config.module = LoraMesher::LoraModules::SX1276_MOD;
    config.freq = 868.0;
    
    radio.begin(config);
    radio.start();
}
```

### After (Heltec V3):

```cpp
#include <Arduino.h>
#include "LoraMesher.h"

// Heltec V3 pins
#define BOARD_LED 35
#define LED_ON HIGH
#define LED_OFF LOW

#define LORA_CS     8
#define LORA_IRQ    14
#define LORA_RST    12
#define LORA_IO1    14

LoraMesher& radio = LoraMesher::getInstance();
SPIClass customSPI(HSPI);

void setup() {
    Serial.begin(115200);
    pinMode(BOARD_LED, OUTPUT);
    
    // Initialize custom SPI
    customSPI.begin(9, 11, 10, 8);
    
    LoraMesher::LoraMesherConfig config;
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    config.loraCs = LORA_CS;
    config.loraIrq = LORA_IRQ;
    config.loraRst = LORA_RST;
    config.loraIo1 = LORA_IO1;
    config.spi = &customSPI;
    config.freq = 868.0;
    
    radio.begin(config);
    radio.start();
}
```

## Key Differences to Note

### 1. SX1262 vs SX1276/1278

The SX1262 offers several improvements:
- Better sensitivity (-148 dBm vs -137 dBm)
- Lower power consumption
- Higher TX power capability (22 dBm vs 20 dBm)
- Different DIO pin configuration

### 2. ESP32-S3 vs ESP32

The ESP32-S3 features:
- More powerful CPU (up to 240 MHz)
- USB-OTG support (native USB)
- More GPIO pins
- Better power management
- Vector instructions for AI/ML

### 3. Custom SPI Configuration

**Why needed**: The Heltec V3 uses non-default SPI pins for the LoRa module to avoid conflicts with the display and other peripherals.

**Important**: Always initialize the custom SPI before calling `radio.begin()`:

```cpp
customSPI.begin(9, 11, 10, 8); // Must be before radio.begin()
```

## Common Migration Issues

### Issue 1: Upload Fails

**Symptom**: Cannot upload to Heltec V3

**Solution**: 
1. Install CP210x USB drivers
2. Hold "PRG" button during upload
3. Try reducing upload speed in platformio.ini:
   ```ini
   upload_speed = 115200
   ```

### Issue 2: LoRa Not Working

**Symptom**: No communication after migration

**Solution**:
1. Verify all nodes use the same LoRa module type (SX1262)
2. Check that all nodes have the same frequency, SF, BW, CR
3. Ensure antenna is connected
4. Verify custom SPI initialization

### Issue 3: Code Compiles but Doesn't Work

**Symptom**: Code uploads but board doesn't respond

**Solution**:
1. Check pin definitions are correct
2. Verify LED polarity (HIGH vs LOW)
3. Add delays after Serial.begin() for USB-CDC:
   ```cpp
   Serial.begin(115200);
   delay(1500); // Wait for USB CDC
   ```

### Issue 4: Mixed Network

**Symptom**: Old boards can't communicate with new Heltec V3

**Solution**: Ensure all boards use compatible settings:
```cpp
// These MUST match on all nodes
config.freq = 868.0;
config.bw = 125.0;
config.sf = 7;
config.cr = 7;
config.syncWord = 0x12;
```

## Testing Migration

### Step-by-step testing:

1. **Upload basic example**
   ```bash
   cd examples/HeltecV3
   pio run -t upload -t monitor
   ```

2. **Verify serial output**
   - Look for "LoRaMesher initialized successfully!"
   - Note the local address (e.g., "Local address: 0xABCD")

3. **Test with existing node**
   - Keep one old board running
   - Upload to Heltec V3
   - Both should see each other in routing table

4. **Verify LED behavior**
   - LED should flash on boot
   - LED should blink on TX/RX

5. **Check display (if using)**
   - Display should show node address
   - Display should update with TX/RX info

## Performance Comparison

| Metric | T-Beam (SX1276) | Heltec V3 (SX1262) |
|--------|-----------------|---------------------|
| Sensitivity | -137 dBm | -148 dBm |
| TX Power | 20 dBm | 22 dBm |
| RX Current | ~10 mA | ~4.2 mA |
| TX Current (14dBm) | ~120 mA | ~120 mA |
| Range (SF7) | ~5 km | ~7 km |
| Range (SF12) | ~15 km | ~20 km |

## Backward Compatibility

### Can Heltec V3 communicate with older boards?

**Yes**, but with considerations:

1. **Same LoRa parameters**: All nodes must use identical settings
2. **Module compatibility**: 
   - SX1262 (Heltec V3) ✅ Compatible with SX1268
   - SX1262 (Heltec V3) ✅ Compatible with SX1276/1278 (if parameters match)
   - SX1262 (Heltec V3) ❌ NOT compatible with SX1280 (different frequency band)

3. **Protocol version**: Ensure all nodes run the same LoRaMesher version

### Example Mixed Network Setup

```cpp
// Configuration that works across SX1262, SX1276, SX1278
config.freq = 868.0;      // Must match
config.bw = 125.0;        // Must match
config.sf = 7;            // Must match
config.cr = 7;            // Must match (4/7)
config.syncWord = 0x12;   // Must match
config.power = 14;        // Can vary per node
config.preambleLength = 8; // Must match
```

## Next Steps

1. **Test basic functionality**: Use `examples/HeltecV3`
2. **Add display**: Upgrade to `examples/HeltecV3-Display`
3. **Optimize power**: Implement sleep modes if battery powered
4. **Adjust parameters**: Fine-tune for your specific use case

## Need Help?

- Check `HELTEC_V3_GUIDE.md` for detailed information
- See `HELTEC_V3_QUICK_REF.md` for quick reference
- Join Discord: https://discord.gg/SSaZhsChjQ
- Open issues on GitHub for bugs

---

**Last Updated**: October 2025  
**Branch**: Heltec-V3
