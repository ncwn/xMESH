/**
 * @file config.h
 * @brief Configuration for Protocol 2 - Hop-Count Routing
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "../common/heltec_v3_pins.h"

// Node Configuration
#ifndef NODE_ID
#define NODE_ID 1  // Default node ID (1-5)
#endif

// Node roles (use XMESH_ prefix to avoid conflict with LoRaMesher library)
#define XMESH_ROLE_SENSOR     0
#define XMESH_ROLE_RELAY      1
#define XMESH_ROLE_GATEWAY    2

// Set node role based on ID
// Updated Nov 14, 2025: Support dual relays (3,4) and dual gateways (5,6)
#if NODE_ID == 5 || NODE_ID == 6
    #define NODE_ROLE XMESH_ROLE_GATEWAY
    #define NODE_ADDRESS (0x0000 + NODE_ID)
#elif NODE_ID == 1 || NODE_ID == 2
    #define NODE_ROLE XMESH_ROLE_SENSOR
    #define NODE_ADDRESS (0x0000 + NODE_ID)
#else  // NODE_ID == 3 or 4
    #define NODE_ROLE XMESH_ROLE_RELAY
    #define NODE_ADDRESS (0x0000 + NODE_ID)
#endif

// LoRa Configuration (AS923 Thailand)
#define LORA_FREQUENCY          923.2      // MHz
#define LORA_BANDWIDTH          125.0      // kHz
#define LORA_SPREADING_FACTOR   7          // SF7 for faster transmission
#define LORA_CODING_RATE        5          // 4/5
#define LORA_SYNC_WORD          0x12       // Private network
#define LORA_TX_POWER           10         // dBm (reduced for indoor testing, forces multi-hop)
#define LORA_PREAMBLE_LENGTH    8          // symbols

// Protocol Configuration - Hop-Count Routing
#define HELLO_INTERVAL_MS       120000     // HELLO packet interval (120 seconds - LoRaMesher default)
#define ROUTE_TIMEOUT_MS        300000     // Route expiration (5 minutes)
#define MAX_ROUTES              10          // Maximum routing table size

// Packet Configuration
#define MAX_PAYLOAD_SIZE        20          // Maximum payload size (bytes)
#define PACKET_INTERVAL_MS      60000      // Send interval for sensors (60 seconds)
#define PACKET_INTERVAL_VARIATION 5000     // Random variation (±5 seconds)
#define GATEWAY_ADDRESS         0x0005     // Fixed gateway address

// Display Configuration
#define DISPLAY_UPDATE_MS       1000       // Display refresh rate
#define DISPLAY_TIMEOUT_MS      30000      // Display sleep timeout
#define DISPLAY_PAGE_SWITCH_MS  5000       // Auto page switch interval

// Logging Configuration
#define SERIAL_BAUD             115200
#define LOG_LEVEL               LOG_INFO   // LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG
#define CSV_OUTPUT              false       // Enable CSV format for data collection
#define LOG_PACKET_CONTENT      false      // Log packet payload (debug)
#define LOG_ROUTING_TABLE       true       // Log routing table updates

// Duty Cycle Configuration
#define DUTY_CYCLE_ENFORCE      true       // Enforce 1% duty cycle limit
#define DUTY_CYCLE_WARNING_PCT  0.83       // Warn at 83% of limit
#define DUTY_CYCLE_CRITICAL_PCT 0.94       // Critical at 94% of limit

// System Configuration
#define WATCHDOG_TIMEOUT_S      30         // Watchdog timer (seconds)
#define HEAP_CHECK_INTERVAL_MS  10000      // Check free heap interval
#define MIN_FREE_HEAP           10240      // Minimum free heap (bytes)

// Debug Configuration
#define DEBUG_LORAMESHER        true       // Enable LoRaMesher debug
#define DEBUG_ROUTING           true       // Enable routing debug
#define DEBUG_RADIO             false      // Enable radio debug
#define DEBUG_MEMORY            false      // Enable memory debug
#define LED_BLINK_ON_TX         true       // Blink LED on transmit
#define LED_BLINK_ON_RX         true       // Blink LED on receive
#define LED_BLINK_ON_HELLO      false      // Blink LED on HELLO packet

// Statistics structure for hop-count routing
struct HopCountStats {
    uint32_t dataPacketsSent;
    uint32_t dataPacketsReceived;
    uint32_t dataPacketsForwarded;
    uint32_t dataPacketsDropped;
    uint32_t helloPacketsSent;
    uint32_t helloPacketsReceived;
    uint32_t routeUpdates;
    uint32_t routeTimeouts;
    float averageRSSI;
    float averageSNR;
    uint8_t currentRouteCount;
    uint8_t averageHopCount;
};

#endif // CONFIG_H
