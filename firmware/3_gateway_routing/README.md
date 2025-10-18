# xMESH Gateway-Aware Cost Routing Protocol - Week 4-5

## Overview

This firmware implements **gateway-aware cost routing** that extends LoRaMesher's hop-count routing with multi-factor cost metrics. Unlike pure hop-count (Baseline 2), this protocol considers link quality, transmission reliability, and gateway load to make smarter routing decisions.

**Key Features:**
- Multi-factor cost function (hop-count + RSSI + SNR + ETX + gateway bias)
- Link quality awareness (RSSI/SNR tracking)
- Transmission reliability (ETX calculation)
- Gateway load balancing (bias penalty)
- Route stability (hysteresis to prevent flapping)

**Hardware:** Heltec WiFi LoRa32 V3 (ESP32-S3 + SX1262)

## Cost Function Design

### Formula

```
cost = W1 × hops + W2 × normalize(RSSI) + W3 × normalize(SNR) + W4 × ETX + W5 × gateway_bias
```

### Components

#### 1. Hop Count (W1 = 1.0)
- **Source:** `RouteNode.networkNode.metric` (from LoRaMesher)
- **Range:** 0-255 hops
- **Weight:** 1.0 (base metric, equal to 1 hop)
- **Rationale:** Maintains compatibility with standard distance-vector routing

#### 2. RSSI - Received Signal Strength Indicator (W2 = 0.3)
- **Source:** `RouteNode.receivedSNR` can be extended to track RSSI
- **Range:** -120 dBm (weak) to -30 dBm (strong)
- **Normalization:** `(RSSI + 120) / 90` → [0, 1]
  - RSSI = -30 dBm → 1.0 (best, minimal cost contribution)
  - RSSI = -120 dBm → 0.0 (worst, maximum cost contribution)
- **Weight:** 0.3 (equivalent to ~0.3 hop penalty for poor link)
- **Rationale:** Prefer stronger links that are more reliable

#### 3. SNR - Signal-to-Noise Ratio (W3 = 0.2)
- **Source:** `RouteNode.receivedSNR` (already tracked by LoRaMesher)
- **Range:** -20 dB (noisy) to +10 dB (clean)
- **Normalization:** `(SNR + 20) / 30` → [0, 1]
  - SNR = +10 dB → 1.0 (best, minimal cost)
  - SNR = -20 dB → 0.0 (worst, maximum cost)
- **Weight:** 0.2 (equivalent to ~0.2 hop penalty for noisy link)
- **Rationale:** Clean signals reduce retransmissions

#### 4. ETX - Expected Transmission Count (W4 = 0.4)
- **Calculation:** `ETX = 1 / delivery_ratio`
- **Example:**
  - 100% delivery → ETX = 1.0 (perfect link)
  - 50% delivery → ETX = 2.0 (needs 2 attempts on average)
  - 25% delivery → ETX = 4.0 (unreliable link)
- **Tracking:** Count sent packets vs acknowledgments per neighbor
- **Weight:** 0.4 (equivalent to ~0.4 hop penalty per extra transmission needed)
- **Rationale:** Accounts for retransmission overhead not captured by RSSI/SNR

#### 5. Gateway Bias (W5 = 1.0)
- **Purpose:** Load balancing across multiple gateways
- **Calculation:** `bias = (gateway_load - avg_load) / avg_load`
- **Example:**
  - Gateway A: 50 packets, Gateway B: 10 packets
  - Average: 30 packets
  - Bias A: (50-30)/30 = +0.67 (penalty)
  - Bias B: (10-30)/30 = -0.67 (bonus)
- **Weight:** 1.0 (equivalent to ~1 hop penalty for overloaded gateway)
- **Rationale:** Distribute traffic to prevent single gateway saturation

### Route Selection with Hysteresis

To prevent **route flapping** (rapid switching between routes), we apply **hysteresis**:

```cpp
// Only switch to new route if cost improvement > 15%
if (new_cost < current_cost * (1 - HYSTERESIS)) {
    switchToNewRoute();
}
```

**Hysteresis = 0.15 (15%)**
- New route must be significantly better to justify switch
- Reduces control overhead from frequent updates
- Maintains route stability

## Comparison with Baselines

| Feature | Flooding (Baseline 1) | Hop-Count (Baseline 2) | Gateway-Aware (Week 4-5) |
|---------|----------------------|------------------------|---------------------------|
| **Routing Metric** | None (broadcast) | Hop count only | Multi-factor cost |
| **Link Quality** | ❌ Ignored | ❌ Ignored | ✅ RSSI + SNR |
| **Reliability** | ❌ No tracking | ❌ No tracking | ✅ ETX |
| **Gateway Load** | ❌ No awareness | ❌ No awareness | ✅ Load balancing |
| **Route Stability** | N/A (stateless) | ❌ No hysteresis | ✅ 15% hysteresis |
| **Overhead** | High (broadcast) | Low (unicast) | Low (unicast) |
| **Scalability** | Poor | Good | Better |

## Implementation Strategy

### Phase 1: Data Collection (Complete in Baseline 2)
- ✅ RSSI/SNR already tracked by LoRaMesher (`RouteNode.receivedSNR`)
- ✅ RTT already tracked (`RouteNode.SRTT`, `RouteNode.RTTVAR`)

### Phase 2: Add Custom Metrics
- ❌ ETX calculation (track TX success rate per neighbor)
- ❌ Gateway load tracking (packet counter per gateway)
- ❌ Link quality history (moving average for stability)

### Phase 3: Modify Route Selection
- ❌ Override LoRaMesher's metric comparison in `RoutingTableService::processRoute()`
- ❌ Calculate combined cost instead of using pure hop-count
- ❌ Apply hysteresis before route updates

### Phase 4: Display Integration
- Protocol indicator: "GW-COST"
- Show: Cost value, ETX, gateway load
- Line 4: `Cost=2.3 ETX=1.2`

## Build Instructions

```bash
cd firmware/3_gateway_routing

# Build for Sensor node
pio run -e sensor

# Build for Router node
pio run -e router

# Build for Gateway node
pio run -e gateway

# Flash and monitor
pio run -e sensor -t upload -t monitor
```

## Configuration

### platformio.ini Build Flags
- `-D XMESH_ROLE_SENSOR` - Generates data packets
- `-D XMESH_ROLE_ROUTER` - Forwards packets only
- `-D XMESH_ROLE_GATEWAY` - Receives and logs packets

### Cost Function Weights (Tunable)

Located in `src/main.cpp`:
```cpp
#define W1_HOP_COUNT    1.0   // Base hop-count weight
#define W2_RSSI         0.3   // RSSI component weight
#define W3_SNR          0.2   // SNR component weight  
#define W4_ETX          0.4   // ETX component weight
#define W5_GATEWAY_BIAS 1.0   // Gateway load penalty weight
#define HYSTERESIS      0.15  // 15% route change threshold
```

**Tuning Guidelines:**
- Increase W2/W3 if link quality is primary concern
- Increase W4 if retransmissions are costly (duty cycle)
- Increase W5 for stronger load balancing
- Increase HYSTERESIS for more stable routes (less flapping)

## Testing Strategy

### Expected Behavior

**Scenario 1: Direct vs Multi-hop**
- Sensor has two paths to Gateway:
  - Path A: Direct (1 hop, RSSI=-80 dBm, SNR=5 dB, ETX=1.2)
  - Path B: Via Router (2 hops, both links RSSI=-50 dBm, SNR=8 dB, ETX=1.0)
- **Hop-count would choose:** Path A (1 hop < 2 hops)
- **Cost routing calculates:**
  - Cost A = 1×1.0 + 0.3×normalize(-80) + 0.2×normalize(5) + 0.4×1.2 = ~2.1
  - Cost B = 1×2.0 + 0.3×normalize(-50) + 0.2×normalize(8) + 0.4×1.0 = ~2.0
- **Cost routing chooses:** Path B (better link quality compensates for extra hop)

**Scenario 2: Gateway Load Balancing**
- Sensor can reach Gateway 1 (50 packets) or Gateway 2 (10 packets)
- Both at 2 hops with similar link quality
- **Cost routing calculates:**
  - Cost G1 = 2.0 + ... + 1.0×(+0.67 bias) ≈ 3.2
  - Cost G2 = 2.0 + ... + 1.0×(-0.67 bias) ≈ 1.8
- **Cost routing chooses:** Gateway 2 (lower load)

### Validation Metrics

Compare with Hop-Count Baseline:
1. **Route stability:** Count route changes over time
2. **Link quality:** Average RSSI/SNR of active routes
3. **Transmission efficiency:** Average ETX across network
4. **Gateway load distribution:** Variance in gateway packet counts
5. **Packet Delivery Ratio:** Should equal or exceed baseline

## Limitations and Future Work

### Current Limitations
1. **RSSI not directly exposed by LoRaMesher** - Need to add tracking
2. **ETX requires acknowledgment tracking** - Need to implement ACK monitoring
3. **Gateway bias needs global view** - May require gateway advertisement
4. **Only works for 1-hop neighbors** - Multi-hop RSSI/SNR not available

### Future Enhancements
1. Adaptive weight tuning based on network conditions
2. Machine learning for optimal weight selection
3. Per-gateway ETX tracking (not just per-neighbor)
4. Congestion avoidance (queue depth consideration)
5. Energy-aware routing (battery level factor)

## References

- LoRaMesher Library: https://github.com/LoRaMesher/LoRaMesher
- ETX Metric: De Couto et al. "A High-Throughput Path Metric for Multi-Hop Wireless Routing" (2003)
- RFC 6551: Routing Metrics for RPL
- Hysteresis in Routing: Prevents oscillation in unstable networks

---

**Version:** v0.4.0-alpha (in development)  
**Status:** Implementation in progress  
**Hardware Tested:** Pending
