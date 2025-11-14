# Hardware Setup Guide - Heltec WiFi LoRa 32 V3

## Hardware Specifications

### Board Details
- **MCU**: ESP32-S3FN8 (Dual-core Xtensa LX7 @ 240 MHz)
- **LoRa Transceiver**: Semtech SX1262
- **Display**: 0.96" OLED (128x64 pixels, SSD1306 controller)
- **Memory**:
  - 512KB SRAM + 8MB PSRAM
  - 8MB Flash
- **Operating Voltage**: 3.3V
- **USB**: USB Type-C with CP2102 USB-to-UART

### Pin Configuration

#### LoRa Module (SX1262)
```cpp
#define LORA_CS_PIN     8   // Chip Select
#define LORA_RST_PIN    12  // Reset
#define LORA_DIO1_PIN   14  // IRQ/DIO1
#define LORA_BUSY_PIN   13  // Busy indicator
#define LORA_MOSI_PIN   10  // SPI MOSI
#define LORA_MISO_PIN   11  // SPI MISO
#define LORA_SCK_PIN    9   // SPI Clock
```

#### OLED Display (SSD1306)
```cpp
#define OLED_SDA_PIN    17  // I2C Data
#define OLED_SCL_PIN    18  // I2C Clock
#define OLED_RST_PIN    21  // Reset
```

#### User Interface
```cpp
#define PRG_BUTTON      0   // Program button (boot mode)
#define VEXT_CTRL       36  // External power control
#define LED_PIN         35  // Onboard LED
```

## Initial Setup

### 1. Driver Installation

#### Windows
1. Download CP210x drivers from Silicon Labs
2. Install drivers before connecting the board
3. Device should appear as "COM#" in Device Manager

#### macOS
1. CP210x drivers usually work out-of-the-box
2. Device appears as `/dev/tty.usbserial-*`
3. If issues occur, install drivers from Silicon Labs

#### Linux
1. CP210x drivers included in kernel (2.6.12+)
2. Device appears as `/dev/ttyUSB*`
3. Add user to dialout group: `sudo usermod -a -G dialout $USER`
4. Logout and login for group changes to take effect

### 2. PlatformIO Installation

```bash
# Install PlatformIO Core
pip install platformio

# Or use the installer script
python -c "$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)"

# Verify installation
pio --version
```

### 3. Board Configuration

Create `platformio.ini` in your firmware directory:

```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi
board_build.arduino.partitions = huge_app.csv
board_upload.flash_size = 8MB
board_build.flash_mode = qio
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L

; Required libraries
lib_deps =
    jgromes/RadioLib @ ^7.1.0
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.5
    adafruit/Adafruit BusIO @ ^1.14.1

; Build flags for Heltec V3
build_flags =
    -D HELTEC_WIFI_LORA_32_V3
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D CORE_DEBUG_LEVEL=0
```

## Hardware Testing

### 1. Basic Connectivity Test

Flash this test program to verify board functionality:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 21
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Heltec WiFi LoRa 32 V3 Test");
    Serial.println("============================");

    // Initialize I2C for OLED
    Wire.begin(17, 18);  // SDA, SCL

    // Initialize display
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Heltec V3 Test");
    display.println("Board: OK");
    display.println("Display: OK");
    display.display();

    Serial.println("Test completed successfully!");
}

void loop() {
    // Blink LED
    static bool ledState = false;
    pinMode(35, OUTPUT);
    digitalWrite(35, ledState);
    ledState = !ledState;
    delay(1000);
}
```

### 2. LoRa Module Test

Test SX1262 connectivity:

```cpp
#include <RadioLib.h>

// SX1262 pins for Heltec V3
SX1262 radio = new Module(8, 14, 12, 13);  // CS, DIO1, RST, BUSY

void setup() {
    Serial.begin(115200);

    // Initialize SX1262
    Serial.print("[SX1262] Initializing ... ");
    int state = radio.begin(923.2, 125.0, 7, 5, 0x12, 14, 8, 0);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("success!");
    } else {
        Serial.print("failed, code ");
        Serial.println(state);
        while (true);
    }
}

void loop() {
    // Transmit test packet
    Serial.print("[SX1262] Transmitting packet ... ");
    int state = radio.transmit("Hello LoRa!");

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("success!");
        Serial.print("[SX1262] Datarate: ");
        Serial.print(radio.getDataRate());
        Serial.println(" bps");
    } else {
        Serial.print("failed, code ");
        Serial.println(state);
    }

    delay(5000);
}
```

## Flashing Firmware

### Using PlatformIO

```bash
# Build firmware
pio run -e heltec_wifi_lora_32_V3

# Upload firmware
pio run -e heltec_wifi_lora_32_V3 --target upload

# Monitor serial output
pio device monitor --baud 115200

# Combined build, upload, and monitor
pio run -e heltec_wifi_lora_32_V3 --target upload && pio device monitor
```

### Manual Flash Mode (if automatic fails)
1. Hold the **PRG** button (GPIO 0)
2. Press and release the **RST** button
3. Release the **PRG** button
4. Board is now in download mode
5. Run upload command

## Power Management

### External Power Control
The V_ext pin (GPIO 36) controls external 3.3V output:

```cpp
#define VEXT_CTRL 36

void enableVext() {
    pinMode(VEXT_CTRL, OUTPUT);
    digitalWrite(VEXT_CTRL, LOW);  // Active LOW
}

void disableVext() {
    pinMode(VEXT_CTRL, OUTPUT);
    digitalWrite(VEXT_CTRL, HIGH);  // Disable external power
}
```

### Battery Operation
- LiPo battery connector available
- Voltage range: 3.7V - 4.2V
- Built-in charging circuit via USB-C
- Battery voltage monitoring via ADC

## Common Issues & Solutions

### Issue 1: Board Not Detected
- **Solution**: Install CP210x drivers, check USB cable (must be data cable, not power-only)

### Issue 2: Upload Fails
- **Solution**: Manually enter flash mode using PRG+RST buttons

### Issue 3: OLED Not Working
- **Solution**: Check I2C address (0x3C or 0x3D), ensure V_ext is enabled

### Issue 4: LoRa Not Initializing
- **Solution**: Verify SPI connections, check if using correct pins for V3 (not V2)

### Issue 5: High Power Consumption
- **Solution**: Disable V_ext when not needed, use deep sleep modes

## Regulatory Compliance (AS923 - Thailand)

### Frequency Plan
- **Uplink**: 923.2 - 923.4 MHz (2 channels)
- **Default Channels**:
  - CH0: 923.2 MHz
  - CH1: 923.4 MHz

### Power Limits
- **Maximum EIRP**: 16 dBm (with 2dBi antenna gain)
- **Conducted Power**: 14 dBm recommended

### Duty Cycle
- **Limit**: 1% (36 seconds per hour)
- **Implementation**: Must track airtime programmatically

```cpp
// Duty cycle tracking
unsigned long totalAirtime = 0;
unsigned long hourStart = millis();

void checkDutyCycle(unsigned long packetAirtime) {
    // Reset counter every hour
    if (millis() - hourStart > 3600000) {
        totalAirtime = 0;
        hourStart = millis();
    }

    totalAirtime += packetAirtime;

    // Check 1% limit (36000ms per hour)
    if (totalAirtime > 36000) {
        Serial.println("WARNING: Duty cycle limit exceeded!");
        // Implement blocking until next hour
    }
}
```

## Node Identification

For multi-node testing, each board needs unique identification:

```cpp
// Generate node address from MAC
uint16_t getNodeAddress() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    return (mac[4] << 8) | mac[5];
}

// Or use manual assignment
#ifdef NODE_1
  #define NODE_ADDRESS 0x0001
#elif NODE_2
  #define NODE_ADDRESS 0x0002
#elif NODE_3
  #define NODE_ADDRESS 0x0003
#endif
```

## Useful Resources

- [Heltec Official Documentation](https://docs.heltec.org/en/node/esp32/index.html)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262)
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/)
- [LoRaMesher Library](https://github.com/LoRaMesher/LoRaMesher)

## Hardware Test Checklist

Before proceeding with protocol implementation:

- [ ] All 5 boards power on successfully
- [ ] USB serial communication works (115200 baud)
- [ ] OLED displays test message
- [ ] LoRa module initializes without errors
- [ ] Basic transmit/receive between 2 boards confirmed
- [ ] Node addresses assigned and verified
- [ ] AS923 frequency compliance verified
- [ ] Duty cycle tracking implemented
- [ ] Battery operation tested (if applicable)
- [ ] Range test completed (indoor environment)

## Safety Notes

1. **Antenna Required**: Never transmit without antenna attached (may damage SX1262)
2. **ESD Protection**: Handle boards with care, use anti-static precautions
3. **Power Limits**: Do not exceed voltage ratings (3.3V logic levels)
4. **Heat Dissipation**: Boards may get warm during continuous transmission
5. **Regulatory Compliance**: Ensure operations comply with local regulations