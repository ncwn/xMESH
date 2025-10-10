# xMESH Flooding Protocol - Baseline Implementation

## Overview

This firmware implements a simple **flooding protocol** as the baseline for xMESH research. In flooding, every node rebroadcasts all received packets to all neighbors. This ensures maximum packet delivery but generates high network overhead.

**Key Features:**
- Role-based behavior (Sensor/Router/Gateway)
- Duplicate detection using a 5-entry circular cache
- Packet sequence numbers to prevent loops
- OLED display showing node status
- Heltec WiFi LoRa32 V3 support (ESP32-S3 + SX1262)

## Protocol Behavior

### Sensor Node (`ROLE_SENSOR`)
- Generates data packets every 60 seconds
- Broadcasts packets to all neighbors
- Rebroadcasts received packets (flooding)
- Tracks TX/RX packet counts

### Router Node (`ROLE_ROUTER`)
- Only forwards packets (no data generation)
- Rebroadcasts all received packets (flooding)
- Implements duplicate detection
- Tracks TX/RX packet counts

### Gateway Node (`ROLE_GATEWAY`)
- Receives and logs packets
- **Does NOT rebroadcast** (sink node)
- Displays packet statistics
- Can be connected to Raspberry Pi for data collection

## Duplicate Detection

To prevent infinite loops, the firmware uses a **5-entry circular cache** that stores:
- Source address
- Sequence number

When a packet arrives:
1. Check if (srcAddr, seqNum) exists in cache
2. If yes: Drop packet (duplicate)
3. If no: Process packet and add to cache

## Hardware Requirements

- **Heltec WiFi LoRa32 V3**
  - ESP32-S3 microcontroller
  - SX1262 LoRa transceiver
  - 0.96" OLED display (128x64)
  - Built-in antenna

## Display Layout

```
Line 1: ID:A3F2 [S]       # Node ID + Role (S/R/G)
Line 2: TX:45 RX:38       # Packet counts
Line 3: FLOOD             # Protocol indicator
Line 4: DC:0.0%           # Duty cycle (placeholder)
```

## Building the Firmware

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install)
2. Clone the xMESH repository

### Build Commands

```bash
cd firmware/1_flooding

# Build for Sensor node
pio run -e sensor

# Build for Router node
pio run -e router

# Build for Gateway node
pio run -e gateway
```

### Upload to Board

```bash
# Upload to sensor (specify port if needed)
pio run -e sensor -t upload

# Upload to router
pio run -e router -t upload

# Upload to gateway
pio run -e gateway -t upload
```

### Monitor Serial Output

```bash
# Monitor serial output (115200 baud)
pio device monitor -b 115200

# Or combine upload + monitor
pio run -e sensor -t upload -t monitor
```

## Configuration

Hardware configuration is defined in `firmware/common/heltec_v3_config.h`:

- **LoRa Frequency:** 923.2 MHz (AS923 Thailand)
- **Spreading Factor:** 7
- **Bandwidth:** 125 kHz
- **TX Power:** 14 dBm
- **Packet Interval:** 60 seconds (sensors)
- **Duty Cycle Limit:** 1% (AS923 regulation)

## Packet Structure

```cpp
struct SensorData {
    uint32_t seqNum;        // Sequence number (duplicate detection)
    uint16_t srcAddr;       // Original source address
    uint32_t timestamp;     // Timestamp (ms since boot)
    float sensorValue;      // Simulated sensor data
    uint8_t hopCount;       // Number of hops from source
};
```

## Testing

### Topology A (3-node linear): S1 → R1 → G1

1. Flash 3 boards:
   - Board 1: Sensor (`pio run -e sensor -t upload`)
   - Board 2: Router (`pio run -e router -t upload`)
   - Board 3: Gateway (`pio run -e gateway -t upload`)

2. Power on all boards

3. Expected behavior:
   - Sensor sends packet every 60s
   - Router rebroadcasts packet
   - Gateway receives and logs packet
   - All nodes show incrementing TX/RX counters

4. Monitor gateway serial output to verify packet delivery

### Serial Output Example

**Sensor:**
```
=================================
xMESH Flooding Protocol
Role: S (SENSOR)
=================================

LoRaMesher initialized
Local address: A3F2

TX: Seq=0 Value=42.35
TX: Seq=1 Value=67.12
```

**Router:**
```
=================================
xMESH Flooding Protocol
Role: R (ROUTER)
=================================

LoRaMesher initialized
Local address: B4E8

RX: Seq=0 From=A3F2 Hops=1 Value=42.35
FLOOD: Rebroadcasting packet 0 from A3F2
```

**Gateway:**
```
=================================
xMESH Flooding Protocol
Role: G (GATEWAY)
=================================

LoRaMesher initialized
Local address: C5D9

RX: Seq=0 From=A3F2 Hops=2 Value=42.35
GATEWAY: Packet 0 from A3F2 received (hops=2, value=42.35)
```

## Known Issues & Warnings

- **Compiler Warning:** `NODE_ROLE_STR redefined` - This is harmless. The LoRaMesher library defines ROLE_GATEWAY internally, which conflicts with our compile flag. Functionality is not affected.
- **ROLE_GATEWAY redefined:** Similar warning from LoRaMesher's BuildOptions.h. Can be safely ignored.

## File Structure

```
firmware/1_flooding/
├── platformio.ini          # Build configuration
├── README.md              # This file
└── src/
    ├── main.cpp           # Main firmware logic
    ├── display.h          # Display interface
    └── display.cpp        # Display implementation

firmware/common/
└── heltec_v3_config.h     # Hardware pin definitions
```

## Performance Metrics (To Be Collected)

This baseline firmware will be tested to collect:
- **PDR (Packet Delivery Ratio):** Percentage of packets reaching gateway
- **Latency:** Time from sensor TX to gateway RX
- **Overhead:** Number of duplicate/forwarded packets
- **Duty Cycle:** LoRa airtime usage

Results will be compared against hop-count and gateway-aware protocols.

## Next Steps

1. Flash firmware to all 5 Heltec boards
2. Test Topology A (3-node linear)
3. Verify OLED display shows correct role and counts
4. Collect baseline metrics for comparison
5. Proceed to Week 3: Hop-Count Routing implementation

## Troubleshooting

### Display not working
- Check I2C address is 0x3C
- Verify OLED_SDA=17, OLED_SCL=18, OLED_RST=21

### No LoRa packets received
- Verify all nodes on same frequency (923.2 MHz)
- Check antenna connections
- Ensure nodes are within range (~100m line-of-sight)

### Upload failed
- Press RESET button while uploading
- Check USB cable and port permissions
- Try `pio run -e sensor -t upload --upload-port /dev/ttyUSB0`

## References

- [Heltec WiFi LoRa32 V3 Docs](https://heltec.org/project/wifi-lora-32-v3/)
- [LoRaMesher Library](https://github.com/LoRaMesher/LoRaMesher)
- [xMESH Proposal](../../proposal_docs/st123843_internship_proposal.md)
- [Project README](../../README.md)

---

**Author:** xMESH Research Project  
**Date:** 2025-10-10  
**Status:** Week 1 - Baseline Implementation Complete
