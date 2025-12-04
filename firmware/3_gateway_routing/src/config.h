/**
 * @file config.h
 * @brief Configuration for Protocol 3 - Gateway-Aware Cost Routing
 */

#ifndef CONFIG_H
#define CONFIG_H

// Include pins first
#include "heltec_v3_pins.h"

// Import LoRaMesher role definitions
#include "BuildOptions.h"

// Node Configuration
#ifndef NODE_ID
#define NODE_ID 1
#endif

// Use LoRaMesher's role values (from BuildOptions.h):
// ROLE_DEFAULT = 0b00000000 (0) - Default/Sensor
// ROLE_GATEWAY = 0b00000001 (1) - Gateway
// For relay, we use ROLE_DEFAULT (sensors and relays both use 0, behavior differs by code)

// Set role based on NODE_ID
// MULTI-GATEWAY/RELAY MODE: Nodes 5 & 6 are gateways, Nodes 3 & 4 are relays
#if NODE_ID == 5 || NODE_ID == 6
    #define NODE_ROLE ROLE_GATEWAY
    #define NODE_ADDRESS (0x0000 + NODE_ID)  // NOTE: UNUSED - Runtime uses MAC-derived address
#else
    #define NODE_ROLE ROLE_DEFAULT
    #define NODE_ADDRESS (0x0000 + NODE_ID)  // NOTE: UNUSED - Runtime uses MAC-derived address
#endif

// IMPORTANT: NODE_ADDRESS defined above is NOT USED at runtime!
// The LoRaMesher library automatically derives addresses from WiFi MAC address:
//   WiFiService::init() extracts last 2 bytes of MAC: (mac[4] << 8) | mac[5]
//   This produces hardware-unique addresses like 0x02B4, 0x6674, 0x8154, etc.
// NODE_ID is only used for determining NODE_ROLE (sensor/relay/gateway behavior)

// Role helper macros
#define IS_SENSOR   (NODE_ID == 1 || NODE_ID == 2)  // Nodes 1, 2 are sensors
#define IS_RELAY    (NODE_ID == 3 || NODE_ID == 4)  // Nodes 3, 4 are relays
#define IS_ROUTER   IS_RELAY
#define IS_GATEWAY  (NODE_ID == 5 || NODE_ID == 6)  // Nodes 5, 6 are gateways

// Role string for display
#if NODE_ID == 5 || NODE_ID == 6
    #define NODE_ROLE_STR "GATEWAY"
#elif NODE_ID == 3 || NODE_ID == 4
    #define NODE_ROLE_STR "RELAY"
#else
    #define NODE_ROLE_STR "SENSOR"
#endif

// SPI pins
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9

// Cost Function Weights
#define W1_HOP_COUNT    1.0
#define W2_RSSI         0.3
#define W3_SNR          0.2
#define W4_ETX          0.4
#define W5_GATEWAY_BIAS 1.0
#define HYSTERESIS_THRESHOLD 0.15

// RSSI/SNR ranges
#define RSSI_MIN        -120
#define RSSI_MAX        -30
#define SNR_MIN         -20
#define SNR_MAX         10

// ETX
#define ETX_WINDOW_SIZE 10
#define ETX_DEFAULT     1.5
#define ETX_ALPHA       0.3

// Trickle
#define TRICKLE_IMIN_MS     60000
#define TRICKLE_IMAX_MS     600000
#define TRICKLE_K           1
#define TRICKLE_ENABLED     true

// Packet
#define PACKET_INTERVAL_MS      60000
#define PACKET_INTERVAL_VARIATION 5000
#define GATEWAY_ADDRESS         0x0005

// Communication Architecture: Bidirectional by Default
// Protocol 3 supports bidirectional routing (gateway ↔ sensor communication)
// LoRaMesher library sends HELLO packets from ALL nodes, enabling full mesh capabilities
// NOTE: Cannot be disabled without modifying LoRaMesher library source
// (Formerly SENSOR_SEND_HELLO flag - removed as it was documentation-only with no code effect)

// RELAY_HAS_SENSOR: Allow relays to also generate data packets
// - false (default): Relay only forwards
// - true: Relay acts as both forwarder AND sensor (dual-role)
#define RELAY_HAS_SENSOR        false   // Enable for multi-function nodes

// Display
#define DISPLAY_UPDATE_MS       1000

// Cost Evaluation
#define COST_EVAL_INTERVAL_MS   300000

// LED
#define LED_BLINK_ON_TX   true
#define LED_BLINK_ON_RX   true

// Statistics
struct CostRoutingStats {
    uint32_t dataPacketsSent = 0;
    uint32_t dataPacketsReceived = 0;
    uint32_t dataPacketsForwarded = 0;
    uint32_t dataPacketsDropped = 0;
};

#endif

// LoRa pin aliases (reference code uses names without _PIN suffix)
#define LORA_CS     LORA_CS_PIN
#define LORA_RST    LORA_RST_PIN
#define LORA_DIO1   LORA_DIO1_PIN
#define LORA_BUSY   LORA_BUSY_PIN

// Bidirectional Routing Behavior
// All nodes (sensors, relays, gateways) send HELLO packets automatically
// This enables gateway→sensor commands for IoT command-and-control applications
// Sensors appear in routing tables with metric reflecting hop distance
