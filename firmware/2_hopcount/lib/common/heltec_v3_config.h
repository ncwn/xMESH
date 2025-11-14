/**
 * @file heltec_v3_config.h
 * @brief Configuration file for Heltec WiFi LoRa32 V3 board
 *
 * This file defines pin mappings and LoRa radio configuration for the
 * Heltec WiFi LoRa32 V3 board (ESP32-S3 + SX1262)
 *
 * Hardware Specifications:
 * - MCU: ESP32-S3FN8 (Dual-Core Xtensa LX7, 240MHz)
 * - LoRa: Semtech SX1262 transceiver
 * - Display: 0.96" OLED 128x64 (SSD1306, I2C)
 * - Frequency: 863-928 MHz
 *
 * References:
 * - Heltec V3 datasheet: https://heltec.org/project/wifi-lora-32-v3/
 * - LoRaMesher SX1262 example: examples/SX1262/
 * - AS923 Thailand regulations: 923.0-923.4 MHz, 1% duty cycle, 16 dBm EIRP max
 *
 * @author xMESH Research Project
 * @date 2025-10-10
 */

#ifndef HELTEC_V3_CONFIG_H
#define HELTEC_V3_CONFIG_H

// ============================================================================
// SX1262 LoRa Transceiver Pin Definitions
// ============================================================================

/**
 * @brief SX1262 SPI Chip Select pin
 * Used to select the SX1262 LoRa chip on the SPI bus
 */
#define LORA_CS     8

/**
 * @brief SX1262 Reset pin
 * Active low reset signal for the LoRa transceiver
 */
#define LORA_RST    12

/**
 * @brief SX1262 DIO1 (Digital I/O 1) pin
 * Used for interrupt-driven packet reception and transmission completion
 */
#define LORA_DIO1   14

/**
 * @brief SX1262 BUSY pin
 * Indicates when the transceiver is busy processing a command
 * Must check before sending new commands to SX1262
 */
#define LORA_BUSY   13

// ============================================================================
// OLED Display Pin Definitions (SSD1306, I2C)
// ============================================================================

/**
 * @brief OLED I2C Data pin (SDA)
 * I2C data line for the SSD1306 OLED display
 */
#define OLED_SDA    17

/**
 * @brief OLED I2C Clock pin (SCL)
 * I2C clock line for the SSD1306 OLED display
 */
#define OLED_SCL    18

/**
 * @brief OLED Reset pin
 * Active low reset signal for the OLED display
 */
#define OLED_RST    21

/**
 * @brief OLED I2C Address
 * Default I2C address for SSD1306 display
 */
#define OLED_ADDR   0x3C

// ============================================================================
// LED Pin Definition
// ============================================================================

/**
 * @brief Built-in LED pin
 * Can be used for status indication
 */
#define LED_PIN     35

// ============================================================================
// LoRa Radio Configuration (AS923 Thailand)
// ============================================================================

/**
 * @brief LoRa Carrier Frequency in MHz
 * AS923 Thailand: 923.0 - 923.4 MHz
 * Using 923.2 MHz as center frequency
 */
#define LORA_FREQ           923.2

/**
 * @brief LoRa Spreading Factor
 * Range: 6-12
 * SF7 = fastest data rate (~5.47 kbps with BW125)
 * Higher SF = longer range but slower data rate
 */
#define LORA_SF             7

/**
 * @brief LoRa Bandwidth in kHz
 * Options: 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500
 * 125 kHz is standard for AS923
 */
#define LORA_BW             125.0

/**
 * @brief LoRa Coding Rate denominator
 * Range: 5-8 (represents 4/5, 4/6, 4/7, 4/8)
 * 5 = 4/5 coding rate (20% overhead, standard)
 */
#define LORA_CR             5

/**
 * @brief LoRa Synchronization Word
 * Used to distinguish different LoRa networks
 * 0x12 = Private network (0x34 is reserved for LoRaWAN)
 */
#define LORA_SYNCWORD       0x12

/**
 * @brief LoRa Transmission Power in dBm
 * Range: 2-22 dBm (SX1262 can output up to 22 dBm)
 * AS923 Thailand limit: 16 dBm EIRP (Equivalent Isotropic Radiated Power)
 * Using 14 dBm to stay safely under limit with antenna gain
 */
#define LORA_POWER          14

/**
 * @brief LoRa Preamble Length in symbols
 * Range: 6-65535
 * Longer preamble = more reliable detection, but more airtime
 * 8 symbols is standard
 */
#define LORA_PREAMBLE       8

/**
 * @brief Maximum packet size in bytes
 * LoRa max physical layer: 255 bytes
 * Keeping at 100 bytes for reliable mesh operation
 * Allows for: 7 bytes header + 93 bytes payload
 */
#define LORA_MAX_PKT_SIZE   100

// ============================================================================
// Duty Cycle Configuration (AS923 Regulation)
// ============================================================================

/**
 * @brief Maximum Duty Cycle percentage
 * AS923 regulation: 1% maximum airtime per hour
 * This equals 36 seconds of transmission per hour
 */
#define DUTY_CYCLE_MAX_PCT  1.0

/**
 * @brief Duty Cycle measurement window in milliseconds
 * 1 hour = 3,600,000 milliseconds
 */
#define DUTY_CYCLE_WINDOW_MS 3600000

/**
 * @brief Maximum airtime per duty cycle window in milliseconds
 * 1% of 1 hour = 36,000 ms = 36 seconds
 */
#define MAX_AIRTIME_MS      36000

// ============================================================================
// Serial Configuration
// ============================================================================

/**
 * @brief Serial baud rate for USB communication
 * Used for debugging and gateway communication (Raspberry Pi)
 */
#define SERIAL_BAUD         115200

// ============================================================================
// Display Configuration
// ============================================================================

/**
 * @brief OLED Display Width in pixels
 */
#define SCREEN_WIDTH        128

/**
 * @brief OLED Display Height in pixels
 */
#define SCREEN_HEIGHT       64

/**
 * @brief Display update interval in milliseconds
 * Updating too frequently causes flicker
 * 2000 ms = update every 2 seconds
 */
#define DISPLAY_UPDATE_INTERVAL_MS 2000

// ============================================================================
// Node Role Definitions (Compile-time configuration)
// ============================================================================

/**
 * @brief Node role definitions based on compile flags
 * Only ONE role should be defined via build flags:
 * -D XMESH_ROLE_GATEWAY, -D XMESH_ROLE_SENSOR, or -D XMESH_ROLE_ROUTER
 * 
 * Note: Using XMESH_ prefix to avoid conflicts with LoRaMesher library's
 * ROLE_GATEWAY definition in BuildOptions.h
 */
#if defined(XMESH_ROLE_GATEWAY)
    #define NODE_ROLE_STR "G"  // Display as [G]
    #define IS_GATEWAY true
    #define IS_SENSOR false
    #define IS_ROUTER false
#elif defined(XMESH_ROLE_SENSOR)
    #define NODE_ROLE_STR "S"  // Display as [S]
    #define IS_GATEWAY false
    #define IS_SENSOR true
    #define IS_ROUTER false
#elif defined(XMESH_ROLE_ROUTER)
    #define NODE_ROLE_STR "R"  // Display as [R]
    #define IS_GATEWAY false
    #define IS_SENSOR false
    #define IS_ROUTER true
#else
    // Default to sensor if no role defined
    #define XMESH_ROLE_SENSOR
    #define NODE_ROLE_STR "S"
    #define IS_GATEWAY false
    #define IS_SENSOR true
    #define IS_ROUTER false
    #warning "No role defined, defaulting to XMESH_ROLE_SENSOR"
#endif

// ============================================================================
// Traffic Pattern Configuration
// ============================================================================

/**
 * @brief Packet generation interval for sensor nodes (milliseconds)
 * 1 packet every 60 seconds = 60000 ms
 * This ensures duty cycle compliance even with multiple hops
 */
#define PACKET_INTERVAL_MS  60000

/**
 * @brief HELLO packet interval (milliseconds)
 * Routing updates broadcast every 30 seconds
 * Used by all nodes to maintain routing tables
 */
#define HELLO_INTERVAL_MS   30000

// ============================================================================
// Sensor Payload Configuration
// ============================================================================

/**
 * @brief Maximum sensor payload size in bytes
 * Total packet = 7 bytes header + payload
 * Keeping payload at 50 bytes for sensor data
 */
#define SENSOR_PAYLOAD_SIZE 50

#endif // HELTEC_V3_CONFIG_H
