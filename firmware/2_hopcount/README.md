# Protocol 2: Hop-Count Routing

[![Protocol](https://img.shields.io/badge/Protocol-Hop--Count-blue.svg)](.)
[![Status](https://img.shields.io/badge/Status-Baseline-green.svg)](.)

Standard distance-vector routing protocol using hop count as the primary metric. This implementation uses the default LoRaMesher library routing mechanism as a comparative baseline against Protocol 1 (flooding) and Protocol 3 (optimized cost routing).

---

## Overview

Protocol 2 implements a **proactive distance-vector routing protocol** where:

- **HELLO packets** are broadcast periodically to discover neighbors
- **Routing tables** are built automatically based on hop count to each destination
- **Data packets** are forwarded via **unicast** to the next hop (not broadcast)
- **Route maintenance** handles topology changes and node failures

This protocol represents the **standard LoRaMesher routing approach** and serves as a performance baseline for evaluating the optimizations in Protocol 3.

### Key Advantages Over Protocol 1
- ✅ **Unicast forwarding**: Only next-hop nodes transmit (reduces network congestion)
- ✅ **Shortest path routing**: Packets traverse minimum hop count
- ✅ **Lower energy consumption**: Fewer redundant transmissions
- ✅ **Better scalability**: Supports larger networks than flooding

### Limitations (Addressed by Protocol 3)
- ⚠️ **Fixed HELLO interval**: Constant overhead regardless of network stability
- ⚠️ **Single metric**: Hop count ignores link quality (RSSI, SNR, ETX)
- ⚠️ **No load balancing**: Cannot distribute traffic across multiple gateways
- ⚠️ **Route flapping**: Sensitive to transient link quality changes

---

## Protocol Operation

### Route Discovery

**HELLO Packet Exchange:**
```
Every 30s (default):
    Node broadcasts HELLO packet containing:
    - Source address
    - Sequence number
    - Neighbor list with hop counts

All neighbors:
    - Receive HELLO
    - Update routing table
    - Calculate hop count = (sender_hop_count + 1)
    - Select route with minimum hop count
```

**Example 3-Hop Network:**
```
Sensor (1) <--HELLO--> Relay (3) <--HELLO--> Gateway (5)

Sensor routing table:
  Dst=5 (Gateway), Via=3, Hops=2

Relay routing table:
  Dst=1 (Sensor), Via=1, Hops=1
  Dst=5 (Gateway), Via=5, Hops=1

Gateway routing table:
  Dst=1 (Sensor), Via=3, Hops=2
  Dst=3 (Relay), Via=3, Hops=1
```

### Data Forwarding

**Sensor to Gateway:**
```
1. Sensor (1) generates data packet
2. Looks up routing table: Gateway (5) via Relay (3)
3. Sends unicast to Relay (3)
4. Relay receives, looks up routing table: Gateway (5) via direct link
5. Relay forwards unicast to Gateway (5)
6. Gateway receives and logs
```

**Key Difference from Flooding:**
- Only **2 transmissions** total (sensor→relay, relay→gateway)
- Flooding would cause **multiple rebroadcasts** from all neighbors

---

## Hardware Requirements

| Component | Specification | Notes |
|-----------|---------------|-------|
| **Microcontroller** | Heltec WiFi LoRa 32 V3 (ESP32-S3) | Required |
| **LoRa Transceiver** | Semtech SX1262 (onboard) | Integrated |
| **Display** | 0.96" OLED 128x64 (onboard) | For status visualization |
| **USB Cable** | USB-C | For programming and power |
| **Power Supply** | 5V USB or 3.7V LiPo battery | 500mA+ recommended |

**Recommended Network Setup:**
- 1-2x Gateway nodes (NODE_ID=5, 6)
- 2-4x Sensor nodes (NODE_ID=1, 2)
- 0-2x Relay nodes (NODE_ID=3, 4) for multi-hop testing

**Multi-Hop Testing:**
- Reduce `LORA_TX_POWER` to 7-10 dBm to force multi-hop indoors
- Place nodes at increasing distances (5m, 10m, 15m intervals)

---

## Software Dependencies

### Required Libraries

All dependencies are automatically installed via PlatformIO:

```ini
lib_deps =
    https://github.com/ncwn/xMESH.git#main        # Modified LoRaMesher library
    adafruit/Adafruit SSD1306 @ ^2.5.7            # OLED display driver
    adafruit/Adafruit GFX Library @ ^1.11.5       # Graphics primitives
    adafruit/Adafruit BusIO @ ^1.14.1             # I2C/SPI abstraction
```

### Build Tools

- **PlatformIO Core** 6.0+ or **PlatformIO IDE** (VS Code extension)
- **Arduino Framework** for ESP32 (auto-installed)
- **ESP32 Platform** (espressif32, auto-installed)

---

## Installation

### 1. Prerequisites

Install PlatformIO:

**Via VS Code:**
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install "PlatformIO IDE" extension from marketplace
3. Restart VS Code

**Via CLI:**
```bash
pip install platformio
```

### 2. Clone Repository

```bash
git clone https://github.com/ncwn/xMESH.git
cd xMESH/firmware/2_hopcount
```

### 3. Configure Node ID

Edit `platformio.ini` to set the node ID:

```ini
build_flags =
    -D HELTEC_WIFI_LORA_32_V3
    -D PROTOCOL_HOPCOUNT
    -D CORE_DEBUG_LEVEL=2
    -I ../common
```

**Then edit `src/config.h`:**

```cpp
#ifndef NODE_ID
#define NODE_ID 1  // Change this: 1-2 (sensor), 3-4 (relay), 5-6 (gateway)
#endif
```

**Node Role Assignment:**
- `NODE_ID=1` or `2`: Sensor nodes (generate data packets)
- `NODE_ID=3` or `4`: Relay nodes (forward packets only)
- `NODE_ID=5` or `6`: Gateway nodes (receive and log)

### 4. Build and Upload

**Via PlatformIO IDE:**
1. Open project folder in VS Code
2. Click "PlatformIO: Upload" (→) in bottom toolbar
3. Monitor output via "PlatformIO: Serial Monitor"

**Via CLI:**
```bash
# Connect Heltec board via USB
pio run --target upload --upload-port /dev/cu.usbserial-0001  # macOS
# pio run --target upload --upload-port /dev/ttyUSB0          # Linux
# pio run --target upload --upload-port COM3                  # Windows

# Monitor serial output
pio device monitor --baud 115200
```

---

## Configuration

### LoRa Parameters (AS923 Thailand)

Default configuration in `src/config.h`:

```cpp
// LoRa Configuration (AS923 Thailand)
#define LORA_FREQUENCY          923.2      // MHz (923.0-923.4 MHz band)
#define LORA_BANDWIDTH          125.0      // kHz
#define LORA_SPREADING_FACTOR   7          // SF7 (highest data rate)
#define LORA_CODING_RATE        5          // 4/5
#define LORA_SYNC_WORD          0x12       // Private network identifier
#define LORA_TX_POWER           10         // dBm (reduced for indoor multi-hop)
#define LORA_PREAMBLE_LENGTH    8          // symbols
```

**Regional Adaptations:**
- **EU868**: Set `LORA_FREQUENCY = 868.1`
- **US915**: Set `LORA_FREQUENCY = 915.0`
- **CN470**: Set `LORA_FREQUENCY = 470.0`

### Protocol Parameters

```cpp
// LoRaMesher Routing Configuration
#define HELLO_INTERVAL_MS       30000      // HELLO packet interval (30 seconds)
#define ROUTE_TIMEOUT_MS        90000      // Route expiry timeout (90 seconds)
#define MAX_ROUTES              10         // Maximum routing table entries
#define MAX_NEIGHBORS           8          // Maximum neighbor table size

// Data Packet Configuration
#define PACKET_INTERVAL_MS      60000      // Sensor transmission interval (60s)
#define MAX_RETRIES             3          // Retransmission attempts
#define ACK_TIMEOUT_MS          2000       // Wait for ACK timeout
```

### Performance Tuning

**For Stable Networks (low mobility):**
```cpp
#define HELLO_INTERVAL_MS 60000   // Reduce HELLO overhead (60s interval)
#define ROUTE_TIMEOUT_MS  180000  // Increase timeout (3 minutes)
```

**For Dynamic Networks (high mobility, node failures):**
```cpp
#define HELLO_INTERVAL_MS 15000   // Faster convergence (15s interval)
#define ROUTE_TIMEOUT_MS  45000   // Quick failure detection (45s)
```

**For Battery-Powered Sensors:**
```cpp
#define PACKET_INTERVAL_MS 300000 // Transmit every 5 minutes
#define LORA_TX_POWER 7           // Reduce power to 7 dBm
```

---

## How to Use

### Basic Operation

1. **Flash firmware** to 3+ nodes with different `NODE_ID` values
2. **Power on gateway first** (NODE_ID=5 or 6)
3. **Wait 60 seconds** for routing table convergence
4. **Power on sensor nodes** (NODE_ID=1 or 2)
5. **Observe OLED display** for routing table updates
6. **Monitor serial output** for packet flows

### Expected Behavior

**Sensor Node Display:**
```
xMESH Hop-Count
Node: 0001 (SNR)
Tx: 12  Rx: 0
Routes: 2 (GW: 2 hops)
Status: Sending...
```

**Relay Node Display:**
```
xMESH Hop-Count
Node: 0003 (RLY)
Tx: 15  Rx: 12
Routes: 3 (Fwd: 12)
Status: Forwarding
```

**Gateway Node Display:**
```
xMESH Hop-Count
Node: 0005 (GW)
Tx: 0   Rx: 12
Routes: 3 (Neighbors: 2)
Status: Receiving
```

### Serial Output Examples

**Sensor Node (NODE_ID=1):**
```
[INFO] Routing table updated: GW=0005 via=0003 hops=2
[INFO] TX: Seq=1 To=0005 Via=0003 Value=23.45
[INFO] Packet sent to 0003 (next hop to 0005)
[INFO] HELLO sent: 1 neighbor(s)
```

**Relay Node (NODE_ID=3):**
```
[INFO] HELLO received from 0001: hops=1
[INFO] HELLO received from 0005: hops=1
[INFO] Routing table: 0001(1), 0005(1)
[INFO] RX: Seq=1 From=0001 To=0005 Value=23.45
[INFO] FWD: Packet to 0005 (next hop, hops=1)
```

**Gateway Node (NODE_ID=5):**
```
[INFO] HELLO received from 0003: hops=1
[INFO] Routing table: 0001(2 via 0003), 0003(1)
[INFO] RX: Seq=1 From=0001 Hops=2 Value=23.45
[INFO] GATEWAY: Packet 1 from 0001 received (hops=2, value=23.45)
CSV,1732370456,0001,0005,1,23.45,-115,9,2
```

---

## Monitoring and Debugging

### Serial Monitor

**View live output:**
```bash
pio device monitor --baud 115200 --filter colorize
```

**Key events to observe:**
- `[INFO] HELLO sent`: Periodic route advertisements
- `[INFO] HELLO received`: Neighbor discovery
- `[INFO] Routing table updated`: Route changes
- `[INFO] TX/RX/FWD`: Packet transmissions

### OLED Display

**Multi-page display (auto-rotates every 5 seconds):**

**Page 1: Statistics**
- Node address and role
- TX/RX/Forward packet counts
- Active routes count
- Gateway hop distance

**Page 2: Routing Table**
- Destination addresses
- Next hop addresses
- Hop counts
- Route quality

**Page 3: System Status**
- Free heap memory
- Duty cycle usage
- Network health
- Last packet timestamp

### Routing Table Inspection

**Enable detailed routing table logging:**

Edit `src/config.h`:
```cpp
#define DEBUG_ROUTING true        // Print routing table every 60s
#define DEBUG_NEIGHBORS true      // Print neighbor list
```

**Example output:**
```
[DEBUG] === Routing Table ===
[DEBUG] Dst=0001 Via=0003 Hops=2 Quality=OK
[DEBUG] Dst=0003 Via=0003 Hops=1 Quality=GOOD
[DEBUG] Dst=0005 Via=0005 Hops=1 Quality=EXCELLENT
[DEBUG] === Neighbors (2) ===
[DEBUG] 0003: RSSI=-85 SNR=8 LastSeen=1234ms
[DEBUG] 0005: RSSI=-92 SNR=6 LastSeen=5678ms
```

---

## Troubleshooting

### Routes Not Building

**Symptom:** Routing table empty after several minutes

**Solutions:**
1. **Wait longer**: Initial convergence takes 60-90 seconds
2. **Check HELLO interval**: Verify `HELLO_INTERVAL_MS` is set (default 30s)
3. **Verify frequency**: All nodes must use same `LORA_FREQUENCY`
4. **Check distance**: Nodes must be within radio range (<100m indoors @ 10dBm)
5. **Monitor HELLO packets**: Look for `[INFO] HELLO received` logs
6. **Power cycle**: Reset all nodes simultaneously for clean start

### Packets Not Reaching Gateway

**Symptom:** Sensor shows routes, but gateway receives nothing

**Check:**
1. **Routing table on sensor**: Must show gateway address with valid next hop
   ```
   [INFO] Routing table: GW=0005 via=0003 hops=2
   ```
2. **Intermediate nodes**: Must have routes to gateway
3. **Serial logs on relay**: Should show `[INFO] FWD:` messages
4. **Gateway reachability**: Try reducing distance or increasing `LORA_TX_POWER`
5. **Packet queue**: Check for `[WARN] Packet queue full` errors

### High Route Flapping

**Symptom:** Routing table constantly changing next hops

**Example:**
```
[INFO] Route to 0005: via=0003 hops=2
[INFO] Route to 0005: via=0004 hops=2  # Changed!
[INFO] Route to 0005: via=0003 hops=2  # Changed again!
```

**Causes:**
- Nodes at similar distances (both 2 hops away)
- Borderline link quality
- HELLO packets experiencing variable SNR

**Solutions:**
```cpp
// Increase route stability
#define ROUTE_TIMEOUT_MS 120000  // Longer timeout
#define HELLO_INTERVAL_MS 45000  // Less frequent updates

// Or upgrade to Protocol 3 which includes:
// - Hysteresis to prevent flapping
// - Multi-metric cost (not just hop count)
// - Link quality consideration
```

### Node Isolation

**Symptom:** `[WARN] No route to gateway` errors

**Debug steps:**

1. **Check neighbor discovery:**
   ```
   [DEBUG] === Neighbors (0) ===  # Problem: No neighbors!
   ```

2. **Verify radio health:**
   ```bash
   pio device monitor | grep "Radio"
   # Look for: [INFO] Radio initialized successfully
   ```

3. **Test direct connectivity:**
   - Move sensor next to gateway (<1m)
   - Should see `[INFO] HELLO received` within 30s

4. **Check TX power:**
   ```cpp
   #define LORA_TX_POWER 14  // Increase for testing
   ```

### Memory Leaks

**Symptom:** `[ERROR] Free heap critically low: <10KB`

**Solutions:**
1. **Reduce routing table size:**
   ```cpp
   #define MAX_ROUTES 8       // Down from 10
   #define MAX_NEIGHBORS 6    // Down from 8
   ```

2. **Clear stale routes:**
   ```cpp
   #define ROUTE_TIMEOUT_MS 60000  # Faster expiry
   ```

3. **Monitor heap trends:**
   ```
   [INFO] Free heap: 45120 bytes  # Should be stable
   [INFO] Free heap: 44896 bytes  # Slow decrease = leak
   [INFO] Free heap: 44672 bytes  # Contact developers!
   ```

---

## Performance Characteristics

### Expected Metrics (5-Node Indoor Network)

| Metric | Typical Value | Notes |
|--------|---------------|-------|
| **Packet Delivery Ratio** | 95-98% | Higher than flooding |
| **Latency** | 100-400ms | Predictable (no random delays) |
| **Energy Consumption** | Medium | Only next-hop transmissions |
| **Convergence Time** | 60-90s | Initial route discovery |
| **Route Flapping** | Moderate | Sensitive to SNR variations |
| **HELLO Overhead** | ~3-5% | Fixed 30s interval |

### Comparison with Other Protocols

| Feature | Protocol 1 (Flood) | Protocol 2 (Hop) | Protocol 3 (Cost) |
|---------|-------------------|-----------------|------------------|
| Routing Method | Broadcast | Unicast | Unicast |
| Metric | None (TTL) | Hop count | Multi-metric cost |
| PDR | 90-95% | 95-98% | **96-100%** |
| HELLO Overhead | N/A | Fixed (100%) | **~30% (Trickle)** |
| Load Balancing | No | No | **Yes (dual-GW)** |
| Link Quality | Ignored | Ignored | **Considered** |
| Scalability | Poor | Good | **Excellent** |

---

## Advanced Usage

### Multi-Hop Validation

**Test 3-hop scenario:**

```bash
# Setup:
# Sensor (1) <--weak--> Relay (3) <--weak--> Relay (4) <--strong--> Gateway (5)

# Reduce TX power to force multi-hop
# In src/config.h:
#define LORA_TX_POWER 7  # Very low power

# Expected routing:
# Node 1: GW=5 via=3 hops=3
# Node 3: GW=5 via=4 hops=2
# Node 4: GW=5 via=5 hops=1
```

**Monitor forwarding chain:**
```
Node 1: [INFO] TX to 0005 via 0003
Node 3: [INFO] FWD from 0001 to 0005 via 0004
Node 4: [INFO] FWD from 0001 to 0005 via 0005
Node 5: [INFO] RX from 0001 (hops=3)
```

### Dual-Gateway Setup

Deploy 2 gateways for redundancy:

```bash
# Flash two gateway nodes
# NODE_ID=5 (Gateway 1)
# NODE_ID=6 (Gateway 2)

# Sensors will route to closest gateway by hop count
# Example:
#   Sensor 1 → Gateway 5 (2 hops)
#   Sensor 2 → Gateway 6 (1 hop)
```

**Limitation:** No load balancing (picks shortest path only).
**Solution:** Use Protocol 3 for active load distribution.

### Custom Data Payloads

Modify `struct SensorPacket` in `src/main.cpp`:

```cpp
struct SensorPacket {
    uint16_t src;           // Source address
    uint16_t sequence;      // Sequence number
    uint32_t timestamp;     // Timestamp
    float temperature;      // Custom sensor 1
    float humidity;         // Custom sensor 2
    uint16_t batteryMv;     // Battery voltage (mV)
    uint8_t hopCount;       // Maintained by LoRaMesher
};
```

**Important:** All nodes must compile with identical structure.

---

## Related Documentation

### Project Documentation
- **[Main README](../../README.md)** - Project overview and setup
- **[Protocol 1: Flooding Baseline](../1_flooding/README.md)** - Broadcast comparison
- **[Protocol 3: Gateway-Aware Routing](../3_gateway_routing/README.md)** - Optimized routing
- **[Raspberry Pi Guide](../../raspberry_pi/README.md)** - Data collection and analysis

### Research Documents
- **[Final Report](../../final_report/main.pdf)** - Complete performance analysis
- **[Experimental Results](../../experiments/)** - Validation test data

### External Resources
- **[LoRaMesher Documentation](https://jaimi5.github.io/LoRaMesher/)** - Library reference
- **[RadioLib Documentation](https://jgromes.github.io/RadioLib/)** - LoRa driver API
- **[Heltec V3 Documentation](https://heltec.org/project/wifi-lora-32-v3/)** - Hardware reference