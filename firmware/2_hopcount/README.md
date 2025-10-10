# xMESH Hop-Count Routing Protocol - Baseline 2

## Overview

This firmware implements **hop-count routing** using LoRaMesher's built-in routing protocol. Unlike the flooding baseline, LoRaMesher automatically manages routing through:
- **Routing table maintenance** via periodic HELLO packets
- **Shortest path selection** based on hop count
- **Intelligent forwarding** to next hop (not broadcast)

**Key Features:**
- Role-based behavior (Sensor/Router/Gateway)
- Automatic route discovery and updates
- Lower network overhead than flooding
- OLED display showing node status
- Heltec WiFi LoRa32 V3 support (ESP32-S3 + SX1262)

## Protocol Behavior

### Sensor Node (`XMESH_ROLE_SENSOR`)
- Generates data packets every 60 seconds
- Broadcasts packets (LoRaMesher routes to gateway)
- Participates in routing (maintains routing table)
- Tracks TX/RX packet counts

### Router Node (`XMESH_ROLE_ROUTER`)
- Only forwards packets (no data generation)
- Maintains routing table via HELLO packets
- Forwards packets using shortest path
- Tracks TX/RX packet counts

### Gateway Node (`XMESH_ROLE_GATEWAY`)
- Receives and logs packets
- **Does NOT forward** (sink node)
- Displays packet statistics
- Can be connected to Raspberry Pi for data collection

## How LoRaMesher Routing Works

### 1. Route Discovery
- All nodes broadcast **HELLO packets** periodically
- HELLO packets contain: node address, hop count from gateway
- Nodes build routing tables based on received HELLOs

### 2. Routing Table
Each node maintains a table with:
- **Destination address** (other nodes)
- **Next hop** (neighbor to forward to)
- **Hop count** (distance to destination)
- **Timestamp** (for route expiry)

### 3. Packet Forwarding
When a packet arrives:
1. Check if destination is self → deliver to application
2. Check routing table for destination → forward to next hop
3. If no route → drop packet

### 4. Route Selection
- LoRaMesher selects **shortest path** (minimum hop count)
- Routes update dynamically as HELLO packets arrive
- Stale routes expire after timeout

## Differences from Flooding Baseline

| Feature | Flooding | Hop-Count Routing |
|---------|----------|-------------------|
| **Forwarding** | Broadcast to all neighbors | Forward to specific next hop |
| **Routing** | None (stateless) | Routing table (stateful) |
| **Overhead** | High (all nodes rebroadcast) | Lower (one forward per node) |
| **Duplicate Detection** | Manual cache (5 entries) | LoRaMesher handles it |
| **Route Discovery** | None | HELLO packets |
| **Scalability** | Poor (broadcast storm) | Better (intelligent routing) |
| **Display Protocol** | "FLOOD" | "HOP-CNT" |

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
Line 3: HOP-CNT           # Protocol indicator
Line 4: DC:0.0%           # Duty cycle (placeholder)
```

## Building the Firmware

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install)
2. Clone the xMESH repository

### Build Commands

```bash
cd firmware/2_hopcount

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
pio run -e sensor -t upload --upload-port /dev/tty.usbserial-0001

# Upload to router
pio run -e router -t upload --upload-port /dev/tty.usbserial-0001

# Upload to gateway
pio run -e gateway -t upload --upload-port /dev/tty.usbserial-0001
```

**To find your port:**
```bash
# macOS/Linux
ls /dev/tty.* | grep -i usb

# Or use PlatformIO
pio device list
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
- **HELLO Interval:** ~30 seconds (LoRaMesher default)
- **Duty Cycle Limit:** 1% (AS923 regulation)

## Packet Structure

```cpp
struct SensorData {
    uint32_t seqNum;        // Sequence number
    uint16_t srcAddr;       // Original source address
    uint32_t timestamp;     // Timestamp (ms since boot)
    float sensorValue;      // Simulated sensor data
    uint8_t hopCount;       // Number of hops from source
};
```

## Testing

### Step-by-Step: Flashing 3 Boards for Topology A

1. **Flash Board 1 as SENSOR:**
   ```bash
   cd firmware/2_hopcount
   pio run -e sensor -t upload --upload-port /dev/tty.usbserial-0001
   ```
   **Verify:** Display shows `ID:XXXX [S]` and "HOP-CNT"

2. **Flash Board 2 as ROUTER:**
   ```bash
   pio run -e router -t upload --upload-port /dev/tty.usbserial-0001
   ```
   **Verify:** Display shows `ID:YYYY [R]` and "HOP-CNT"

3. **Flash Board 3 as GATEWAY:**
   ```bash
   pio run -e gateway -t upload --upload-port /dev/tty.usbserial-0001
   ```
   **Verify:** Display shows `ID:ZZZZ [G]` and "HOP-CNT"

4. **Power all 3 boards** and wait ~30-60 seconds for routing tables to build

5. **Monitor gateway serial output:**
   ```bash
   pio device monitor -b 115200 -p /dev/tty.usbserial-0001
   ```

### Expected Behavior

**Startup (0-60 seconds):**
- All nodes broadcast HELLO packets
- Routing tables gradually populate
- Serial output shows: "LoRaMesher will automatically send HELLO packets..."

**After 60 seconds:**
- Sensor sends first data packet
- Router checks routing table → forwards to gateway
- Gateway receives and logs packet
- TX/RX counters increment on displays

**Key Differences from Flooding:**
- Router TX counter increases **less** (only forwards, doesn't broadcast)
- No duplicate messages (LoRaMesher prevents loops)
- Packets take shortest path to gateway

### Topology A (3-node linear): S1 → R1 → G1

**Expected Behavior:**
1. All nodes send HELLO packets periodically
2. Sensor generates packet every 60s
3. Router forwards packet to gateway (via routing table)
4. Gateway receives and logs packet
5. No duplicate packets (unlike flooding)

### Serial Output Example

**Sensor:**
```
=================================
xMESH Hop-Count Routing Protocol
Role: S (SENSOR)
=================================

LoRaMesher initialized with hop-count routing
Local address: A3F2
Routing table will be built automatically via HELLO packets

TX: Seq=0 Value=42.35
```

**Router:**
```
=================================
xMESH Hop-Count Routing Protocol
Role: R (ROUTER)
=================================

LoRaMesher initialized with hop-count routing
Local address: B4E8
Routing table will be built automatically via HELLO packets

RX: Seq=0 From=A3F2 Hops=1 Value=42.35
```

**Gateway:**
```
=================================
xMESH Hop-Count Routing Protocol
Role: G (GATEWAY)
=================================

LoRaMesher initialized with hop-count routing
Local address: C5D9
Routing table will be built automatically via HELLO packets

RX: Seq=0 From=A3F2 Hops=2 Value=42.35
GATEWAY: Packet 0 from A3F2 received (hops=2, value=42.35)
```

## Performance Comparison with Flooding

| Metric | Flooding | Hop-Count | Expected Improvement |
|--------|----------|-----------|---------------------|
| **Network Overhead** | High (all rebroadcast) | Lower (single forward) | ~50-70% reduction |
| **PDR** | High (redundant paths) | High (shortest path) | Similar or better |
| **Latency** | Low (parallel) | Slightly higher | Negligible |
| **Scalability** | Poor (broadcast storm) | Better (routing table) | Significant |
| **Duty Cycle** | High | Lower | Better compliance |

## Troubleshooting

### Display not working
**Already fixed** in flooding baseline - Vext pin (GPIO 36) set LOW to power OLED.

### Role not working correctly
Use `XMESH_ROLE_*` build flags (not `ROLE_*`) to avoid conflicts with LoRaMesher library.

### No packets received
- **Wait 60 seconds** for routing tables to build via HELLO packets
- Verify all nodes on same frequency (923.2 MHz)
- Check antenna connections
- Monitor serial output for "Routing table will be built..."

### Router not forwarding
- Check routing table has route to gateway
- Enable debug output: `-D CORE_DEBUG_LEVEL=5` in platformio.ini
- Monitor serial for routing table updates

### HELLO packets not seen
- LoRaMesher sends HELLOs automatically (~30s interval)
- HELLOs are internal to LoRaMesher (not visible in app layer)
- Check LoRaMesher library documentation for timing

## File Structure

```
firmware/2_hopcount/
├── platformio.ini          # Build configuration
├── README.md              # This file
└── src/
    ├── main.cpp           # Main firmware logic
    ├── display.h          # Display interface
    └── display.cpp        # Display implementation

firmware/common/
└── heltec_v3_config.h     # Hardware pin definitions
```

## Implementation Notes

### Key Code Differences from Flooding

**Flooding (manual rebroadcast):**
```cpp
// Manual forwarding in flooding
radio.createPacketAndSend(BROADCAST_ADDR, data, 1);
txCount++;
```

**Hop-Count (LoRaMesher handles it):**
```cpp
// LoRaMesher automatically forwards based on routing table
// No manual forwarding needed!
// TX counter only increments when sensor generates packets
```

### LoRaMesher API Usage

```cpp
// Initialize with automatic routing enabled
radio.begin(config);
radio.start();  // Enables HELLO packet broadcasting

// Send packet - LoRaMesher routes automatically
radio.createPacketAndSend(BROADCAST_ADDR, &data, 1);

// Receive packet - LoRaMesher delivers after routing
AppPacket<SensorData>* packet = radio.getNextAppPacket<SensorData>();
```

## Performance Metrics (To Be Collected)

This hop-count firmware will be tested to collect:
- **PDR (Packet Delivery Ratio):** Percentage of packets reaching gateway
- **Latency:** Time from sensor TX to gateway RX
- **Overhead:** Number of forwarded packets vs flooding
- **Route Stability:** How often routes change
- **Duty Cycle:** LoRa airtime usage

Results will be compared against flooding and gateway-aware protocols.

## Next Steps

1. ✅ Implement hop-count routing
2. ✅ Compile all 3 build environments
3. 🔄 Test on hardware (Topology A)
4. 📊 Compare metrics with flooding baseline
5. ➡️ Proceed to Week 4: Gateway-Aware Cost Routing implementation

## References

- [Heltec WiFi LoRa32 V3 Docs](https://heltec.org/project/wifi-lora-32-v3/)
- [LoRaMesher Library](https://github.com/LoRaMesher/LoRaMesher)
- [LoRaMesher Counter Example](../../examples/Counter/)
- [xMESH Proposal](../../proposal_docs/st123843_internship_proposal.md)
- [Project README](../../README.md)

---

**Author:** xMESH Research Project  
**Date:** 2025-10-10  
**Status:** Week 2/3 - Hop-Count Baseline Implementation Complete

**Build Status:** ✅ All 3 environments compile successfully  
**Hardware Testing:** Pending
