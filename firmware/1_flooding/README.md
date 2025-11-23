# Protocol 1: Flooding Baseline

[![Protocol](https://img.shields.io/badge/Protocol-Flooding-orange.svg)](.)
[![Status](https://img.shields.io/badge/Status-Baseline-blue.svg)](.)

Simple broadcast-based flooding protocol for LoRa mesh networks. This implementation serves as a baseline for demonstrating the scalability limitations of broadcast approaches in duty-cycle constrained networks.

---

## Overview

Protocol 1 implements a naive flooding approach where nodes rebroadcast all received packets to ensure network-wide propagation. While simple to implement, this protocol demonstrates the fundamental scalability challenges of broadcast-based routing:

- **Broadcast Storm Problem**: All intermediate nodes rebroadcast, causing network congestion
- **No Path Optimization**: Packets may traverse unnecessarily long routes
- **High Energy Consumption**: Every node transmits for every unique packet
- **Limited Scalability**: Performance degrades rapidly as network size increases

This protocol serves as the **control baseline** for comparing against optimized routing protocols (Protocol 2 and Protocol 3).

---

## Protocol Operation

### Packet Flow

1. **Sensor Nodes**: Generate data packets every 60 seconds with incrementing sequence numbers
2. **Relay/Router Nodes**: Rebroadcast all received packets (except duplicates) after random delay
3. **Gateway Nodes**: Receive and log packets, terminate the flood (no rebroadcast)

### Duplicate Detection

Packets are uniquely identified by `(source_address, sequence_number)` tuple:

```cpp
struct SensorPacket {
    uint16_t src;           // Source address
    uint16_t sequence;      // Sequence number
    uint8_t ttl;            // Time to live (hop limit)
    uint32_t timestamp;     // Packet creation time
    float sensorValue;      // Simulated sensor data
};
```

Duplicate cache stores the last `DUPLICATE_CACHE_SIZE` (default: 50) packets with automatic timeout after 30 seconds.

### Time-To-Live (TTL)

- Initial TTL: 5 hops (configurable via `MAX_TTL`)
- Decremented at each hop
- Packet dropped when TTL reaches 0
- Prevents infinite loops in network

---

## Hardware Requirements

| Component | Specification | Notes |
|-----------|---------------|-------|
| **Microcontroller** | Heltec WiFi LoRa 32 V3 (ESP32-S3) | Required |
| **LoRa Transceiver** | Semtech SX1262 (onboard) | Integrated |
| **Display** | 0.96" OLED 128x64 (onboard) | For status visualization |
| **USB Cable** | USB-C | For programming and power |
| **Power Supply** | 5V USB or 3.7V LiPo battery | 500mA+ recommended |

**Minimum Network Setup:**
- 1x Gateway node (NODE_ID=5 or 6)
- 1x Sensor node (NODE_ID=1 or 2)
- Optional: Relay nodes (NODE_ID=3 or 4) for multi-hop testing

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
cd xMESH/firmware/1_flooding
```

### 3. Configure Node ID

Edit `platformio.ini` to set the node ID:

```ini
build_flags =
    -D HELTEC_WIFI_LORA_32_V3
    -D PROTOCOL_FLOODING
    -D NODE_ID=1              # Change this: 1-2 (sensor), 3-4 (relay), 5-6 (gateway)
    -D CORE_DEBUG_LEVEL=2
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
// Protocol Configuration - Flooding
#define MAX_TTL                 5           // Maximum hop count (prevent infinite loops)
#define DUPLICATE_CACHE_SIZE    50          // Number of recent packets to track
#define DUPLICATE_TIMEOUT_MS    30000       // Forget duplicates after 30 seconds
#define REBROADCAST_DELAY_MIN   0           // Minimum random delay (ms)
#define REBROADCAST_DELAY_MAX   100         // Maximum random delay (ms)
#define PACKET_INTERVAL_MS      60000       // Sensor transmission interval (60s)
```

### Node-Specific Tuning

**For Dense Networks (many nodes):**
- Increase `REBROADCAST_DELAY_MAX` to 200-500ms (reduce collisions)
- Reduce `MAX_TTL` to 3-4 (limit broadcast storm)

**For Sparse Networks (few nodes):**
- Decrease `REBROADCAST_DELAY_MAX` to 50ms (faster propagation)
- Increase `MAX_TTL` to 6-8 (ensure coverage)

**For Battery-Powered Nodes:**
- Increase `PACKET_INTERVAL_MS` to 120000+ (reduce transmissions)
- Reduce `LORA_TX_POWER` to 7-10 dBm (conserve energy)

---

## How to Use

### Basic Operation

1. **Flash firmware** to 3+ nodes with different `NODE_ID` values
2. **Power on gateway first** (NODE_ID=5 or 6)
3. **Power on sensor nodes** (NODE_ID=1 or 2)
4. **Observe OLED display** for real-time status
5. **Monitor serial output** for detailed logs

### Expected Behavior

**Sensor Node Display:**
```
xMESH Flooding
Node: 0001 (SNR)
Tx: 12  Rx: 0
Dup: 0  Rebroadcast: 0
Status: Sending...
```

**Gateway Node Display:**
```
xMESH Flooding
Node: 0005 (GW)
Tx: 0   Rx: 45
Dup: 12 Rebroadcast: 0
Status: Packet RX
```

### Serial Output Examples

**Sensor Node (NODE_ID=1):**
```
[INFO] TX: Seq=1 To=FFFF TTL=5 Value=23.45
[INFO] Packet sent (250 bytes, 45ms airtime)
[DEBUG] Duty cycle: 0.75% (18s/1800s)
```

**Relay Node (NODE_ID=3):**
```
[INFO] RX: Seq=1 From=0001 TTL=4 Value=23.45
[DEBUG] Not duplicate, rebroadcasting in 73ms
[INFO] REBROADCAST: Seq=1 From=0001 TTL=3
```

**Gateway Node (NODE_ID=5):**
```
[INFO] RX: Seq=1 From=0001 TTL=3 Value=23.45
[INFO] GATEWAY: Packet 1 from 0001 received (hops=2, value=23.45)
CSV,1732370456,0001,0005,1,23.45,-118,8,2
```

---

## Monitoring and Debugging

### Serial Monitor

**View live output:**
```bash
pio device monitor --baud 115200 --filter colorize
```

**Common log levels:**
- `[ERROR]`: Critical failures (radio init, memory errors)
- `[WARN]`: Non-critical issues (duty cycle warnings)
- `[INFO]`: Normal operation (packet TX/RX, statistics)
- `[DEBUG]`: Detailed protocol behavior (duplicate detection, cache updates)

### OLED Display

**Multi-page display (auto-rotates every 5 seconds):**

**Page 1: Statistics**
- Node address and role
- TX/RX packet counts
- Duplicate detections
- Rebroadcast count

**Page 2: Network Status**
- Free heap memory
- Duty cycle usage
- Last packet timestamp
- Current status message

### Data Collection

**Enable CSV output** for analysis:

Edit `src/config.h`:
```cpp
#define CSV_OUTPUT true  // Enable CSV format
```

**CSV Format:**
```
CSV,timestamp,src,dst,seq,value,rssi,snr,hops
CSV,1732370456,0001,0005,1,23.45,-118,8,2
```

**Capture to file:**
```bash
pio device monitor > logs/node1_$(date +%Y%m%d_%H%M%S).log
```

---

## Troubleshooting

### Radio Initialization Failed

**Symptom:** `[ERROR] Radio initialization failed: -2`

**Solutions:**
1. Check USB connection and power supply
2. Verify LoRa antenna is connected
3. Reset board (press RST button)
4. Re-flash firmware with `pio run --target upload`
5. Check for hardware damage or incorrect board selection

### No Packets Received

**Symptom:** Gateway shows `Rx: 0` after several minutes

**Check:**
1. **Frequency match**: All nodes must use same `LORA_FREQUENCY`
2. **Sync word**: Must match across network (`LORA_SYNC_WORD`)
3. **Distance**: Start with nodes <10m apart for testing
4. **Power levels**: Increase `LORA_TX_POWER` to 14 dBm for outdoor
5. **Antenna**: Ensure antennas are properly connected
6. **Serial logs**: Check for `[ERROR]` messages on sensor nodes

### High Duplicate Count

**Symptom:** Gateway shows `Dup: >50%` of received packets

**Expected behavior in flooding:**
- Duplicates are normal (packets arrive via multiple paths)
- Typical: 30-60% duplicates in 4-5 node network
- High duplicates (>80%) may indicate network congestion

**Mitigations:**
1. Reduce `MAX_TTL` to limit broadcast radius
2. Increase `REBROADCAST_DELAY_MAX` for collision avoidance
3. Lower `LORA_TX_POWER` to reduce interference range

### Duty Cycle Violations

**Symptom:** `[WARN] Duty cycle exceeded: 1.2% (limit: 1.0%)`

**Causes:**
- Too many nodes transmitting simultaneously
- `PACKET_INTERVAL_MS` too short
- High duplicate rebroadcasts

**Solutions:**
```cpp
// Increase sensor interval
#define PACKET_INTERVAL_MS 90000  // 90 seconds instead of 60

// Reduce TTL to limit rebroadcasts
#define MAX_TTL 3  // Down from 5
```

**Legal compliance:**
- AS923: 1% duty cycle (36s/hour transmission)
- EU868: 1% duty cycle
- US915: No duty cycle limit (but FCC power limits apply)

### Out of Memory

**Symptom:** `[ERROR] Free heap critically low: 8192 bytes`

**Solutions:**
1. Reduce `DUPLICATE_CACHE_SIZE` to 30
2. Disable debug logging: `#define LOG_LEVEL LOG_INFO`
3. Increase `DUPLICATE_TIMEOUT_MS` to clear cache faster
4. Monitor heap: `[INFO]` logs show free heap every 10 seconds

---

## Performance Characteristics

### Expected Metrics (3-5 Node Indoor Network)

| Metric | Typical Value | Notes |
|--------|---------------|-------|
| **Packet Delivery Ratio** | 90-95% | Lower than optimized protocols |
| **Duplicate Rate** | 30-60% | High due to broadcast nature |
| **Latency** | 200-800ms | Variable due to random delays |
| **Energy Consumption** | High | All nodes transmit for all packets |
| **Scalability** | Poor | Degrades rapidly >5 nodes |

### Comparison with Protocol 2/3

Protocol 1 is **intentionally inefficient** to demonstrate baseline:

| Feature | Protocol 1 | Protocol 2 | Protocol 3 |
|---------|-----------|-----------|-----------|
| Routing | Broadcast | Hop-count | Cost-based |
| Overhead | Very High | Medium | Low (Trickle) |
| PDR | 90-95% | ~97% | 96-100% |
| Multi-hop | Limited | Yes | Optimized |
| Scalability | Poor | Good | Excellent |

---

## Advanced Usage

### Multi-Gateway Setup

Deploy 2+ gateways for redundancy:

```bash
# Flash two gateway nodes
pio run --target upload --upload-port /dev/ttyUSB0  # Gateway 1 (NODE_ID=5)
# Edit platformio.ini: NODE_ID=6
pio run --target upload --upload-port /dev/ttyUSB1  # Gateway 2 (NODE_ID=6)
```

Both gateways will receive packets (duplicates expected).

### Custom Payload

Modify `struct SensorPacket` in `src/main.cpp`:

```cpp
struct SensorPacket {
    uint16_t src;
    uint16_t sequence;
    uint8_t ttl;
    uint32_t timestamp;
    float temperature;      // Custom field
    float humidity;         // Custom field
    uint16_t batteryMv;     // Custom field
};
```

**Important:** All nodes must use identical packet structure.

### Integration with MQTT

Connect gateway serial output to Raspberry Pi running `raspberry_pi/serial_collector.py`:

```bash
python3 serial_collector.py --port /dev/ttyUSB0 --mqtt-broker localhost
```

Publishes to MQTT topic: `loramesher/node/<node_id>/data`

---

## Related Documentation

### Project Documentation
- **[Main README](../../README.md)** - Project overview and setup
- **[Protocol 2: Hop-Count Routing](../2_hopcount/README.md)** - Standard metric-based routing
- **[Protocol 3: Gateway-Aware Routing](../3_gateway_routing/README.md)** - Optimized cost routing
- **[Raspberry Pi Guide](../../raspberry_pi/README.md)** - Data collection and analysis

### Research Documents
- **[Final Report](../../final_report/main.pdf)** - Complete research analysis
- **[Experimental Results](../../experiments/)** - Validation test data

### External Resources
- **[LoRaMesher Documentation](https://jaimi5.github.io/LoRaMesher/)** - Base library
- **[RadioLib Documentation](https://jgromes.github.io/RadioLib/)** - LoRa driver
- **[Heltec V3 Documentation](https://heltec.org/project/wifi-lora-32-v3/)** - Hardware specs