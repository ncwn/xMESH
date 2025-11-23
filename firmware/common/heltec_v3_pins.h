/**
 * @file heltec_v3_pins.h
 * @brief Pin definitions for Heltec WiFi LoRa 32 V3 board
 *
 * Hardware: ESP32-S3 with SX1262 LoRa transceiver and SSD1306 OLED
 */

#ifndef HELTEC_V3_PINS_H
#define HELTEC_V3_PINS_H

// LoRa SX1262 Pin Definitions
#define LORA_CS_PIN     8   // Chip Select (NSS)
#define LORA_RST_PIN    12  // Reset
#define LORA_DIO1_PIN   14  // DIO1/IRQ for SX1262
#define LORA_BUSY_PIN   13  // Busy indicator (SX1262 specific)
#define LORA_MOSI_PIN   10  // SPI Master Out Slave In
#define LORA_MISO_PIN   11  // SPI Master In Slave Out
#define LORA_SCK_PIN    9   // SPI Clock

// OLED Display Pin Definitions (I2C)
#define OLED_SDA_PIN    17  // I2C Data
#define OLED_SCL_PIN    18  // I2C Clock
#define OLED_RST_PIN    21  // OLED Reset
#define OLED_ADDRESS    0x3C // I2C Address

// Board Control Pins
#define VEXT_CTRL_PIN   36  // External 3.3V power control (active LOW)
#define LED_PIN         35  // Onboard LED
#define PRG_BUTTON      0   // Program/Boot button (GPIO 0)

// Battery Monitoring (if using battery)
#define BATTERY_ADC_PIN 37  // ADC pin for battery voltage

// OLED Display Configuration
#define SCREEN_WIDTH    128 // OLED display width in pixels
#define SCREEN_HEIGHT   64  // OLED display height in pixels

// Helper Macros
#define ENABLE_VEXT()   digitalWrite(VEXT_CTRL_PIN, LOW)
#define DISABLE_VEXT()  digitalWrite(VEXT_CTRL_PIN, HIGH)
#define LED_ON()        digitalWrite(LED_PIN, HIGH)
#define LED_OFF()       digitalWrite(LED_PIN, LOW)

// SPI Configuration for LoRa
#define LORA_SPI_FREQUENCY  8000000  // 8 MHz SPI clock

// Default LoRa Configuration for AS923 (Thailand)
#define DEFAULT_LORA_FREQUENCY  923.2  // MHz
#define DEFAULT_LORA_BANDWIDTH  125.0  // kHz
#define DEFAULT_LORA_SF         7       // Spreading Factor
#define DEFAULT_LORA_CR         5       // Coding Rate 4/5
#define DEFAULT_LORA_SYNC_WORD  0x12   // Private network
#define DEFAULT_LORA_TX_POWER   14     // dBm (max 16 for AS923)
#define DEFAULT_LORA_PREAMBLE   8      // Preamble length

// Board Identification
#define BOARD_NAME      "Heltec WiFi LoRa 32 V3"
#define BOARD_VARIANT   "ESP32-S3"
#define LORA_CHIP       "SX1262"

#endif // HELTEC_V3_PINS_H