/**
 * @file config.h
 * @brief xMESH Production Firmware Configuration
 * 
 * This file contains all configurable parameters for the xMESH routing stack.
 * Values are ported from the research prototype (Protocol 3) config.h.
 */

#ifndef XMESH_CONFIG_H
#define XMESH_CONFIG_H

#include <cstdint>

// ============================================================
// Trickle Scheduler Parameters (RFC 6206)
// ============================================================
// Reduces control overhead by 30-40% in stable networks

constexpr uint32_t TRICKLE_I_MIN = 60000;      // Minimum interval: 60s
constexpr uint32_t TRICKLE_I_MAX = 600000;     // Maximum interval: 600s (10 min)
constexpr uint8_t TRICKLE_K = 1;               // Redundancy constant (suppress if ANY neighbor sent HELLO)
constexpr bool TRICKLE_ENABLED = true;         // Enable Trickle adaptive scheduling

// ============================================================
// Cost Router Weights (Multi-Metric Routing)
// ============================================================
// Formula: cost = W1*hops + W2*(1-norm_RSSI) + W3*(1-norm_SNR) + W4*ETX + W5*gateway_bias

constexpr float W1_HOP_COUNT = 1.0f;           // Weight for hop count
constexpr float W2_RSSI = 0.3f;                // Weight for RSSI
constexpr float W3_SNR = 0.2f;                 // Weight for SNR
constexpr float W4_ETX = 0.4f;                 // Weight for ETX (Expected Transmission Count)
constexpr float W5_GATEWAY_BIAS = 1.0f;        // Weight for gateway load balancing

// RSSI/SNR normalization ranges (for cost calculation)
constexpr int16_t RSSI_MIN = -120;             // Minimum RSSI (dBm)
constexpr int16_t RSSI_MAX = -30;              // Maximum RSSI (dBm)
constexpr int8_t SNR_MIN = -20;                // Minimum SNR (dB)
constexpr int8_t SNR_MAX = 10;                 // Maximum SNR (dB)

// ============================================================
// ETX Tracker Parameters (Zero-Overhead Link Quality)
// ============================================================
// Uses sequence-gap detection to track link quality without probe packets

constexpr uint8_t ETX_WINDOW_SIZE = 10;        // Sliding window size for ETX calculation
constexpr float ETX_DEFAULT = 1.5f;            // Default ETX for new links
constexpr float ETX_ALPHA = 0.3f;              // EWMA smoothing factor (0.0-1.0)

// ============================================================
// Gateway Balancer Parameters (Load Balancing)
// ============================================================
// Distributes traffic across multiple gateways to prevent bottlenecks

constexpr uint32_t MIN_GATEWAY_LOAD_WINDOW_MS = 1000;       // Minimum sampling window (1s)
constexpr float LOAD_SWITCH_THRESHOLD = 0.25f;              // Min pkt/min delta for load-based switch
constexpr uint8_t MAX_GATEWAY_CANDIDATES = 10;              // Max gateways to evaluate

// Neighbor health monitoring thresholds
constexpr uint32_t DETECTION_THRESHOLD_MS = 360000;         // 6 min (miss 2 safety HELLOs @ 180s)
constexpr uint32_t WARNING_THRESHOLD_MS = 180000;           // 3 min (miss 1 safety HELLO)

// ============================================================
// Hardware Configuration (Heltec WiFi LoRa 32 V3)
// ============================================================

// LoRa Radio Parameters
constexpr uint32_t LORA_FREQUENCY = 923000000;              // AS923 frequency (Hz)
constexpr uint8_t LORA_BANDWIDTH = 125;                     // Bandwidth: 125 kHz
constexpr uint8_t LORA_SPREADING_FACTOR = 7;                // SF7 (faster, shorter range)
constexpr uint8_t LORA_CODING_RATE = 5;                     // CR 4/5
constexpr int8_t LORA_TX_POWER = 20;                        // TX power: 20 dBm

// OLED Display (SSD1306) - Heltec V3 uses internal I2C pins (17, 18)
// Display.cpp handles initialization with correct Heltec V3 pin mapping
constexpr uint8_t OLED_RST = 21;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;

// ============================================================
// Sensor Configuration (PMS7003 Air Quality + GPS)
// ============================================================

// PMS7003 Particulate Sensor (UART)
constexpr uint8_t PMS_RX_PIN = 4;                // PMS7003 TX -> ESP32 RX
constexpr uint8_t PMS_TX_PIN = 5;                // ESP32 TX -> PMS7003 RX (optional, for commands)
constexpr uint32_t PMS_BAUD = 9600;              // PMS7003 baud rate

// GPS Module (UART) - NEO-M8N or similar
constexpr uint8_t GPS_RX_PIN = 6;                // GPS TX -> ESP32 RX
constexpr uint8_t GPS_TX_PIN = 7;                // ESP32 TX -> GPS RX (optional)
constexpr uint32_t GPS_BAUD = 9600;              // GPS baud rate (NEO-M8N default)

// Sensor enable flags
constexpr bool ENABLE_PMS_SENSOR = true;         // Enable air quality sensing
constexpr bool ENABLE_GPS_SENSOR = true;         // Enable GPS location

// ============================================================
// Sensor Power Management
// ============================================================

constexpr uint8_t PMS_SET_PIN = 3;
constexpr uint32_t SENSOR_READ_INTERVAL_MS = 60000;
constexpr uint32_t PMS_WARMUP_MS = 30000;
constexpr uint32_t PMS_DETECT_TIMEOUT_MS = 3000;
constexpr uint32_t GPS_DETECT_TIMEOUT_MS = 2000;

// ============================================================
// MQTT Configuration (Gateway Only)
// ============================================================

constexpr const char* MQTT_BROKER_DEFAULT = "";
constexpr uint16_t MQTT_PORT_DEFAULT = 1883;
constexpr const char* MQTT_TOPIC_PREFIX = "xmesh/sensors";
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 30000;
constexpr bool ENABLE_MQTT_FORWARD = true;

// ============================================================
// Node Configuration
// ============================================================

// Set to true if this node is a gateway (has WiFi/Internet uplink)
constexpr bool IS_GATEWAY_NODE = false;

// Node address (0 = auto-assign from LoRaMesher)
constexpr uint16_t NODE_ADDRESS = 0;

// ============================================================
// Debugging and Logging
// ============================================================

constexpr uint32_t SERIAL_BAUD = 115200;

#endif // XMESH_CONFIG_H
