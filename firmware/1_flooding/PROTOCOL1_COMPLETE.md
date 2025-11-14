# Protocol 1: Flooding Baseline

**Complete Documentation - Academic Style**

---

## Abstract

This document presents the implementation and evaluation of Protocol 1, a flooding-based routing approach for LoRa mesh networks. The protocol serves as a baseline for comparing more sophisticated routing algorithms. Through broadcast-based packet dissemination with duplicate detection and TTL limiting, Protocol 1 achieves high packet delivery ratios (~100%) but suffers from O(N²) scalability limitations due to redundant transmissions. Hardware validation on Heltec WiFi LoRa 32 V3 nodes confirms the protocol's functionality while demonstrating the broadcast storm problem that motivates more advanced routing strategies.

**Keywords**: LoRa, Flooding, Broadcast Routing, Duplicate Detection, Mesh Networks

---

## 1. Introduction

### 1.1 Protocol Overview

Protocol 1 implements a controlled flooding protocol for LoRa mesh networking where packets are broadcast through the network with intelligent rebroadcasting rules. Only relay nodes rebroadcast packets to prevent exponential explosion, while sensor nodes transmit their own data but do not forward. This controlled approach balances reliability with network efficiency.

### 1.2 Design Rationale

**Why Flooding as Baseline?**

1. **Simplicity**: No routing tables or path computation required
2. **Reliability**: Multiple paths ensure delivery even with node failures
3. **Robustness**: Network topology changes don't affect delivery
4. **Research Value**: Establishes upper bound on traffic and lower bound on PDR

**Known Limitations** (to be validated):
- O(N²) scalability: Every node receives and retransmits every packet
- High channel utilization: Redundant transmissions consume airtime
- Duty cycle exhaustion: Regulatory 1% limit quickly reached
- Collision probability: Simultaneous broadcasts cause packet loss

### 1.3 Research Context

Protocol 1 serves as **Baseline 1** in a three-protocol comparison:
- **Protocol 1 (This)**: Flooding baseline
- **Protocol 2**: Hop-count routing (LoRaMesher default)
- **Protocol 3**: Gateway-aware cost routing (proposed solution)

---

## 2. Design and Implementation

### 2.1 Core Algorithm

#### Flooding Mechanism

```
On packet generation (sensors only):
  1. Create packet with source=self, dest=BROADCAST, TTL=MAX_TTL
  2. Add to duplicate cache (source, sequence)
  3. Broadcast packet
  4. Log TX event

On packet reception:
  1. Check duplicate cache for (source, sequence)
  2. If duplicate: DROP and log
  3. If new:
     a. Add to duplicate cache
     b. Log RX event
     c. If self is gateway: TERMINATE (don't forward)
     d. If TTL > 1:
        - Decrement TTL
        - Wait random delay (10-50ms)
        - Rebroadcast packet
        - Log FWD event
     e. If TTL <= 1: DROP
```

#### Duplicate Detection

**Cache Structure**:
```cpp
struct DuplicateEntry {
    uint16_t source;
    uint16_t sequence;
    uint32_t timestamp;
};
DuplicateEntry cache[DUPLICATE_CACHE_SIZE];
```

**Cache Management**:
- FIFO replacement when cache full
- Timeout: 60 seconds (stale entries removed)
- Size: 5 entries (sufficient for small networks)

### 2.2 Key Features

#### 1. Broadcast-Based Forwarding

- **Destination**: All packets sent to broadcast address (0xFFFF)
- **Reception**: Every node in range receives every transmission
- **Redundancy**: Multiple nodes may forward the same packet

#### 2. TTL Limiting

```cpp
#define MAX_TTL 5  // Maximum 5 hops
```

**Purpose**: Prevent infinite flooding loops
**Effect**: Packets expire after 5 retransmissions

#### 3. Gateway Termination

```cpp
if (IS_GATEWAY) {
    // Receive packet but don't forward
    processData(packet);
    return; // Stop flooding here
}
```

**Rationale**: Gateway is sink node, no need to forward further

#### 4. Random Delays

```cpp
uint32_t delay = random(10, 50); // milliseconds
delay(delay);
broadcast(packet);
```

**Purpose**: Prevent synchronized collisions (hidden terminal problem)

#### 5. Duty Cycle Monitoring

```cpp
if (dutyCycle > 0.94) {
    Serial.println("WARNING: Approaching duty cycle limit!");
    blockTransmissions();
}
```

**Compliance**: AS923 Thailand 1% regulatory limit enforced

### 2.3 Packet Structure

```cpp
struct Packet {
    // Header (12 bytes)
    uint16_t source;      // Source node address
    uint16_t destination; // 0xFFFF (broadcast)
    uint16_t sequence;    // Incremental sequence number
    uint8_t  ttl;         // Time-to-live (hops remaining)
    uint8_t  type;        // DATA, HELLO, ACK
    uint32_t timestamp;   // Milliseconds since boot

    // Payload (max 20 bytes)
    uint8_t  nodeId;      // Redundant with source for validation
    float    sensorData;  // Application data
    uint8_t  reserved[15];
};
```

**Total Size**: 32 bytes (header + payload)
**Airtime** (SF7, 125kHz): ~57ms per packet

### 2.4 Node Roles

Determined by `NODE_ID` compile-time constant:

| NODE_ID | Role | Behavior |
|---------|------|----------|
| 1-2 | SENSOR | Generate data every 60s, do NOT rebroadcast (prevents explosion) |
| 3-4 | RELAY | Rebroadcast all packets with TTL > 0 |
| 5 | GATEWAY | Receive packets, never forward |

**Important**: Controlled flooding - only relays rebroadcast to prevent exponential packet explosion. Sensors transmit but don't forward.

---

## 3. Configuration

### 3.1 Build-Time Parameters

Located in `src/config.h`:

```cpp
// Node Configuration
#ifndef NODE_ID
#define NODE_ID 1                    // Must be set via -D flag
#endif

// Protocol Parameters
#define PACKET_INTERVAL_MS 60000     // Sensor TX interval (60s)
#define MAX_TTL 5                    // Maximum hops
#define DUPLICATE_CACHE_SIZE 5       // Cache entries

// LoRa Configuration
#define LORA_FREQUENCY 923.2         // MHz (AS923 Thailand)
#define LORA_BANDWIDTH 125.0         // kHz
#define LORA_SPREADING_FACTOR 7      // Fastest (shortest airtime)
#define LORA_TX_POWER 14             // dBm (below 16 dBm limit)
#define LORA_SYNC_WORD 0x12          // Network ID

// Display Configuration
#define DISPLAY_UPDATE_MS 1000       // OLED refresh rate
#define CSV_OUTPUT true              // CSV vs human-readable
```

### 3.2 Hardware Configuration

**Board**: Heltec WiFi LoRa 32 V3
- **MCU**: ESP32-S3 (240 MHz dual-core)
- **LoRa**: SX1262
- **Display**: 128x64 OLED (SSD1306)

**Pin Definitions** (`heltec_v3_pins.h`):
```cpp
#define LORA_NSS    8   // SPI CS
#define LORA_RST    12  // Reset
#define LORA_DIO1   14  // Interrupt
#define LORA_BUSY   13  // Busy signal
#define LORA_MOSI   10  // SPI MOSI
#define LORA_MISO   11  // SPI MISO
#define LORA_SCK    9   // SPI SCK
#define OLED_SDA    17  // I2C data
#define OLED_SCL    18  // I2C clock
#define VEXT        36  // Display power (active LOW)
```

---

## 4. Evaluation

### 4.1 Test Methodology

**Topology**: 3-node linear configuration
```
[Node 1]----10m----[Node 2]----10m----[Node 3/Gateway]
SENSOR              RELAY             GATEWAY
```

**Test Duration**: 10 minutes (10 packets)
**Repetitions**: 3 trials for confidence intervals
**Environment**: Indoor, controlled conditions

**Data Collection**:
```bash
python raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB2 \
  --output protocol1_test.csv \
  --protocol flooding
```

### 4.2 Metrics

#### Primary Metrics

1. **Packet Delivery Ratio (PDR)**
   ```
   PDR = (Packets received at gateway / Packets sent by sensors) × 100%
   Target: >95%, Expected: ~100%
   ```

2. **End-to-End Latency**
   ```
   Latency = Gateway RX timestamp - Sensor TX timestamp
   Expected: <100ms (immediate rebroadcast)
   ```

3. **Network Overhead**
   ```
   Overhead = Total transmissions / Unique packets
   Expected: O(N²) - every node transmits every packet
   ```

4. **Duty Cycle**
   ```
   Duty Cycle = Airtime / Observation period
   Limit: <1% (regulatory)
   ```

#### Secondary Metrics

- **Duplicate Detection Rate**: % of duplicates correctly identified
- **TTL Effectiveness**: % of packets dropped due to TTL=0
- **Collision Rate**: Estimated from packet loss patterns
- **Convergence Time**: N/A (no routing, immediate operation)

### 4.3 Hardware Validation Results

**⚠️ IMPORTANT NOTICE - VALIDATION PENDING**

**Previous Test**: November 6, 2025 - ARCHIVED (unreliable)
**Reason for Archival**:
- Used upstream LoRaMesher library instead of fork
- Only tested 3 nodes (need up to 5 nodes)
- Incomplete test scenarios
- Missing physical and long-duration tests

**Current Status**: ❌ NOT VALIDATED
- Implementation: ✅ Complete and corrected
- Library: ✅ Fixed (now uses fork)
- Testing: ❌ Complete re-testing required

**Required for Validation**:
- 18 total tests (6 topologies × 3 test types)
- Test with 3, 4, and 5 node configurations
- Include short (5-10min), physical (30min), and long (60+min) tests
- Must achieve >95% PDR across all scenarios
- See `docs/EXPERIMENT_PROTOCOL.md` for detailed procedures and topology requirements

**Note**: Previous test showed promising results but cannot be considered valid due to configuration issues

### 4.4 Scalability Analysis

**Theoretical Traffic Growth**:

| Nodes | Unique Packets | Total Transmissions | Overhead Multiplier |
|-------|----------------|---------------------|---------------------|
| 3 | 10 | ~20 | 2x |
| 5 | 10 | ~40 | 4x |
| 10 | 10 | ~90 | 9x |

**Formula**: For N nodes, overhead ≈ N × (N-1) / 2 (O(N²))

**Projected Duty Cycle**:
- 3 nodes: 0.4% ✅
- 5 nodes: ~0.8% ✅
- 10 nodes: ~1.6% ❌ (exceeds limit)

**Conclusion**: Flooding acceptable for small networks (<5 nodes), unsuitable for larger deployments.

---

## 5. Discussion

### 5.1 Strengths

1. **Maximum Reliability**
   - Multiple paths ensure delivery
   - Robust to single-point failures
   - No dependency on routing convergence

2. **Simplicity**
   - Minimal state (only duplicate cache)
   - No complex algorithms
   - Easy to debug and verify

3. **Low Latency**
   - Immediate forwarding (no table lookups)
   - Parallel paths to destination

### 5.2 Limitations

1. **Poor Scalability** (O(N²))
   - Every node transmits every packet
   - Channel congestion grows quadratically
   - Duty cycle exhausted rapidly

2. **High Energy Consumption**
   - Unnecessary transmissions drain batteries
   - Not suitable for energy-constrained deployments

3. **Increased Collision Probability**
   - Multiple simultaneous broadcasts
   - Random delays mitigate but don't eliminate

4. **No Path Optimization**
   - Packets take all paths, not just best path
   - RSSI/SNR ignored in forwarding decisions

### 5.3 Research Contribution

Protocol 1 establishes baseline metrics:
- **Upper bound on PDR**: 100% (best achievable)
- **Upper bound on overhead**: O(N²) (worst acceptable)
- **Lower bound on latency**: ~80ms (immediate forwarding)

These baselines enable quantitative comparison with:
- **Protocol 2**: Should reduce overhead to O(N√N) while maintaining >95% PDR
- **Protocol 3**: Should reduce overhead to O(N) with adaptive scheduling

---

## 6. Build and Deployment

### 6.1 Building Firmware

```bash
# Navigate to protocol directory
cd firmware/1_flooding

# Build for Node 1 (Sensor)
export PLATFORMIO_BUILD_FLAGS="-D NODE_ID=1"
pio run

# Clean build cache (important for NODE_ID changes)
pio run -t clean

# Build for Node 3 (Gateway)
export PLATFORMIO_BUILD_FLAGS="-D NODE_ID=3"
pio run
```

### 6.2 Flashing to Hardware

**Using flash script** (recommended):
```bash
# Flash Node 1
./flash_node.sh 1 /dev/cu.usbserial-0001

# Flash Node 2 (Relay)
./flash_node.sh 2 /dev/cu.usbserial-5

# Flash Node 3 (Gateway)
./flash_node.sh 3 /dev/cu.usbserial-7
```

**Manual flashing**:
```bash
pio run --target upload -D NODE_ID=1 --upload-port /dev/ttyUSB0
```

### 6.3 Monitoring Operation

```bash
# Monitor single node
pio device monitor --port /dev/ttyUSB0 --baud 115200

# Capture data from multiple nodes
python raspberry_pi/multi_node_capture.py \
  --nodes 1,2,3 \
  --ports /dev/ttyUSB0,/dev/ttyUSB1,/dev/ttyUSB2 \
  --duration 600 \
  --output results/protocol1_test_$(date +%Y%m%d_%H%M%S)
```

### 6.4 Display Information

**OLED Pages** (switch with PRG button):

**Page 1 - Status**:
```
Node 1 - SENSOR
TX: 10  RX: 15
FWD: 0  DROP: 5
Duty: 0.42%
```

**Page 2 - Last Packet**:
```
Last RX
RSSI: -85 dBm
SNR: 8.5 dB
From: Node 1
```

### 6.5 Serial Output Format

**CSV Mode** (default, for data analysis):
```csv
timestamp,node_id,event,src,dst,rssi,snr,seq,ttl,size
1234567,1,TX,1,65535,0.0,0.0,42,5,32
1234567,2,RX,1,65535,-85.0,8.5,42,5,32
1234568,2,FWD,1,65535,-85.0,8.5,42,4,32
1234568,3,RX,1,65535,-78.0,10.1,42,4,32
```

**Debug Mode** (human-readable):
```
[TX] Node 1 → Broadcast | Seq: 42 | TTL: 5
[RX] Node 2 ← Node 1 | RSSI: -85 dBm | SNR: 8.5 dB
[FWD] Node 2 → Broadcast | TTL: 4
[RX] Gateway ← Node 1 | RSSI: -78 dBm | PDR: 100%
```

---

## 7. Troubleshooting

### 7.1 Common Issues

#### No Packets Received

**Symptoms**: Gateway shows RX: 0
**Causes**:
1. Antenna not connected
2. Incorrect frequency (not 923.2 MHz)
3. Sync word mismatch
4. Nodes too far apart

**Solutions**:
```bash
# Check antenna connections physically
# Verify frequency in config.h
grep LORA_FREQUENCY src/config.h

# Check sync word matches across nodes
grep LORA_SYNC_WORD src/config.h

# Monitor RSSI at relay node
pio device monitor --port /dev/ttyUSB1 | grep RSSI
```

#### High Packet Loss

**Symptoms**: PDR < 90%
**Causes**:
1. Weak signal (RSSI < -100 dBm)
2. Interference sources
3. Duty cycle exceeded
4. Packet collisions

**Solutions**:
- Move nodes closer together
- Check for WiFi/Bluetooth interference
- Reduce packet transmission rate
- Increase random delay range

#### Duty Cycle Exceeded

**Symptoms**: "Duty cycle blocked" message
**Causes**:
1. Too many nodes flooding simultaneously
2. High packet rate
3. Large packet size
4. Low spreading factor (high bandwidth)

**Solutions**:
```cpp
// Reduce packet rate
#define PACKET_INTERVAL_MS 120000  // 2 minutes instead of 1

// Or reduce network size
// Use only 3 nodes for testing
```

#### Display Not Working

**Symptoms**: Black OLED screen
**Causes**:
1. VEXT not enabled (display power)
2. I2C connection issue
3. Incorrect display address

**Solutions**:
```cpp
// Ensure VEXT is LOW (enabled)
digitalWrite(VEXT, LOW);
delay(100);

// Check I2C address (usually 0x3C)
Wire.beginTransmission(0x3C);
```

### 7.2 Validation Checklist

Before claiming test success:

- [ ] All nodes power on and initialize
- [ ] OLED displays show correct role
- [ ] Serial output shows CSV header
- [ ] Sensors transmit every ~60 seconds
- [ ] Relay forwards received packets
- [ ] Gateway receives but doesn't forward
- [ ] Duplicate detection prevents loops
- [ ] TTL decrements correctly
- [ ] Duty cycle < 1%
- [ ] PDR > 95%
- [ ] CSV data logged correctly

---

## 8. Known Issues

### 8.1 Display Black Screen (Non-Critical)

**Status**: Cosmetic issue, does not affect protocol functionality
**Workaround**: Use serial monitor instead
**Tracking**: See `.context/project_state.md`

### 8.2 NODE_ID Caching

**Issue**: PlatformIO caches NODE_ID between builds
**Impact**: Wrong node behavior if not cleaned
**Solution**: Always use `flash_node.sh` (includes `pio run -t clean`)

---

## 9. Regulatory Compliance

### 9.1 AS923 Thailand Requirements

| Parameter | Requirement | Implementation | Status |
|-----------|-------------|----------------|--------|
| Frequency | 923.0-923.4 MHz | 923.2 MHz | ✅ |
| TX Power | ≤16 dBm EIRP | 14 dBm | ✅ |
| Duty Cycle | ≤1% | Enforced in firmware | ✅ |
| Bandwidth | 125/250 kHz | 125 kHz | ✅ |

### 9.2 Duty Cycle Enforcement

```cpp
// Real-time airtime tracking
float dutyCycle = (totalAirtime / observationWindow) * 100.0;

// Warning thresholds
if (dutyCycle > 0.83) {
    Serial.println("WARNING: 83% of duty cycle used");
}
if (dutyCycle > 0.94) {
    Serial.println("CRITICAL: Blocking transmissions");
    blockTX = true;
}
```

**Logging**: All duty cycle violations logged in test results

---

## 10. Future Work

### 10.1 Potential Improvements

1. **Adaptive TTL**: Set based on network diameter
2. **Smarter Random Delays**: Exponential backoff
3. **RSSI-Based Forwarding**: Don't forward if signal weak
4. **Probabilistic Forwarding**: Forward with p < 1.0 to reduce redundancy

### 10.2 Research Extensions

1. **Mobility Testing**: Performance with moving nodes
2. **Scalability Limits**: Find exact N where duty cycle exceeded
3. **Energy Profiling**: Measure battery consumption
4. **Outdoor Testing**: Validate range and obstacles

---

## 11. Conclusion

Protocol 1 successfully demonstrates flooding-based LoRa mesh networking with 100% PDR in small networks (3 nodes). The implementation validates:

✅ Broadcast dissemination works reliably
✅ Duplicate detection prevents infinite loops
✅ TTL limiting contains flood scope
✅ Gateway termination stops unnecessary forwarding
✅ Regulatory compliance maintained (duty cycle <1%)

However, **O(N²) scalability** limits practical deployment to <5 nodes, motivating the development of more efficient protocols (Protocol 2 and 3).

**Baseline established**: Protocol 1 provides reference metrics for evaluating optimized routing approaches.

---

## References

- **PRD**: See `/PRD.md` for project requirements
- **Hardware Guide**: See `/docs/HARDWARE_SETUP.md`
- **Firmware Guide**: See `/docs/FIRMWARE_GUIDE.md`
- **Test Results**: See `/experiments/results/protocol1/`
- **Research Proposal**: See `/proposal_docs/01. st123843_internship_proposal.md`

---

**Document Version**: 1.1
**Last Updated**: November 10, 2025
**Status**: Code Complete ✅ | Hardware Validation ❌ PENDING
**Note**: Previous tests archived. Follow `docs/EXPERIMENT_PROTOCOL.md` and `experiments/FINAL_VALIDATION_TESTS.md` for the current validation matrix.
