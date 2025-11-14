# Critical Validation Tests: Multi-Hop and Multi-Gateway

**Purpose:** Validate the remaining gaps for Protocol 3
**Priority:** Multi-hop test (critical), multi-gateway test (validated but documented here for repeatability)
**Time Required:** 2-3 hours total
**Updated:** November 13, 2025

---

### Status Summary (Nov 13, 2025)
- **Multi-hop spacing** → still pending. All indoor runs keep hops=0, so the hallway/parking-lot test below remains the top priority.
- **Multi-gateway load sharing** → ✅ Completed. Keep the procedure documented so we can rerun quickly if reviewers request fresh data. Reference logs: `experiments/results/protocol3/w5_gateway_indoor_20251113_013301/` (30-min cold start) and `.../w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` (35-min endurance with node outages).

## Test 1: Physical Distance Multi-Hop Validation (CRITICAL)

**Duration:** 1.5 hours (setup + test + analysis)
**Priority:** HIGHEST - Required for "mesh network" claim

### Why This is Critical

**Current Gap:**
- All indoor tests: hops=0 (direct paths)
- Cannot claim multi-hop mesh validation
- Relay forwarding necessity not demonstrated
- Cost-based route selection not observable

**This Test Validates:**
- Multi-hop routing (hops>1)
- Relay forwarding is required (not just available)
- Cost differences observable (weak direct vs strong relay path)
- Protocol 3's multi-metric advantage

---

### Test Configuration: 3-Node with Physical Separation

**Topology:**
```
[Sensor1]----10-15m----[Relay3]----10-15m----[Gateway5]
```

**Physical Setup:**
- Sensor1: One end of hallway/outdoor area
- Relay3: Middle position
- Gateway5: Opposite end
- **Total distance:** 20-30 meters end-to-end
- **Spacing:** 10-15m between nodes (prevents direct sensor→gateway)

**Power Configuration:**
- **Option A (Recommended):** 6-8 dBm all nodes
  - Forces multi-hop with 10-15m spacing
  - Safe range: 5-10m per hop
- **Option B (Aggressive):** 4-6 dBm
  - Even more forced multi-hop
  - May have reliability issues

**Monitoring:**
- Gateway (Node 5): USB required (PDR measurement)
- Relay (Node 3): USB optional but recommended
- Sensor (Node 1): Battery (or USB if 3rd port available)

---

### Flash Sequence (6 dBm Power)

**Step 1: Edit TX Power** (5 minutes)
```bash
# Edit all 3 protocol configs
vim firmware/1_flooding/src/config.h
# Change: LORA_TX_POWER 10 → 6

vim firmware/2_hopcount/src/config.h
# Change: LORA_TX_POWER 10 → 6

vim firmware/3_gateway_routing/src/heltec_v3_pins.h
# Change: DEFAULT_LORA_TX_POWER 10 → 6
```

**Step 2: Flash Protocol 1** (10 min)
```bash
cd firmware/1_flooding
./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Step 3: Position Nodes** (5 min)
- Place Sensor1 at position A
- Place Relay3 at 10-15m from Sensor1
- Place Gateway5 at 10-15m from Relay3
- Verify OLED displays on all nodes

**Step 4: Run Test** (30 min)
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --node2-port /dev/cu.usbserial-5 \
  --duration 1800 \
  --protocol protocol1 \
  --test-name "3node_physical_multihop_6dBm"
```

**Repeat for Protocol 2 and Protocol 3**

---

### Success Criteria for Multi-Hop Test

**Must Observe:**
- hops>0 in gateway logs (NOT hops=0!)
- Relay forwarding activity
- Sensor cannot reach gateway directly
- PDR: Aim for >90% (may be lower than indoor due to distance)

**Gateway Routing Table Should Show:**
```
Sensor1 | Relay3 | 2 | 00  ← 2 hops (via relay)
NOT:
Sensor1 | Sensor1 | 1 | 00  ← 1 hop (direct)
```

**Protocol 3 Specific:**
- Cost calculation with different values
- Relay path cost vs direct path cost (if weak direct exists)
- Route selection based on cost, not just hops

---

### Expected Results

| Protocol | PDR | Hops | Multi-Hop Validated |
|----------|-----|------|-------------------|
| Protocol 1 | 85-95% | N/A | Yes (broadcast through relay) |
| Protocol 2 | 85-95% | 2 | Yes (via relay required) |
| **Protocol 3** | **90-100%** | **2** | **Yes (cost-based via relay)** |

**If Protocol 3 PDR > Protocol 1/2:** Demonstrates superiority even in multi-hop

**If all similar PDR:** Still validates multi-hop mesh, focus on overhead reduction

---

## Test 2: Multi-Gateway Topology (CRITICAL for Proposal)

**Duration:** 1 hour
**Priority:** HIGH - Validates gateway-aware routing claim

### Why This is (Now) Critical for Documentation

- ✅ **Evidence captured** (Nov 13). `w5_gateway_indoor_20251113_013301` shows sensors alternating gateways with `[W5] Load-biased gateway selection …` logs and 16 vs 13 packets delivered per gateway. `w5_gateway_indoor_over_I600s_node_detection_20251113_120301` adds 35 minutes of failure cycling with constant load rebalance.
- 📄 **Documentation requirement**: Keep the exact steps below so we can reproduce the data set if reviewers or the thesis committee request a fresh run (e.g., after firmware tweaks).
- 🧪 **Optional repeat**: Run again if we change HELLO serialization or tweak the bias formula; otherwise, cite the existing logs.

---

### Test Configuration: 5-Node with 2 Gateways

**Topology:**
```
[Sensor1]---[Sensor2]---[Relay3]---[Gateway4]
                            \
                            [Gateway5]
```

**Node Assignments:**
| Node | ID | Role | Power | Monitoring |
|------|-----|------|-------|------------|
| Node 1 | 1 | Sensor | 10 dBm | Battery or USB |
| Node 2 | 2 | Sensor | 10 dBm | Battery |
| Node 3 | 3 | Relay | 10 dBm | Battery |
| **Node 4** | **4** | **Gateway 1** | 10 dBm | **USB** |
| **Node 5** | **5** | **Gateway 2** | 10 dBm | **USB** |

**Key Change:** Configure Node 4 as GATEWAY (not relay)

---

### Configuration Change Required

**File:** `firmware/3_gateway_routing/src/config.h` (lines 26-38)

**Current:**
```cpp
#if NODE_ID == 5
    #define NODE_ROLE XMESH_ROLE_GATEWAY
```

**Modify to:**
```cpp
#if NODE_ID == 4 || NODE_ID == 5
    #define NODE_ROLE XMESH_ROLE_GATEWAY
```

**This makes Node 4 a gateway** instead of relay.

---

### Flash Sequence (Multi-Gateway)

```bash
cd firmware/3_gateway_routing

# Edit config.h first to include Node 4 as gateway
vim src/config.h
# Change line 26: #if NODE_ID == 5
# To: #if NODE_ID == 4 || NODE_ID == 5

# Flash all nodes
./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 4 /dev/cu.usbserial-6  # Now GATEWAY
./flash_node.sh 5 /dev/cu.usbserial-7  # GATEWAY
```

**Verify Node 4 OLED:** Should show "GATEWAY Node: 4" (not RELAY)

---

### Run Multi-Gateway Test

```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol3 \
  --test-name "5node_multigateway"
```

**Monitor both gateways** (Nodes 4 & 5)

---

### Success Criteria for Multi-Gateway Test

**Must Observe in Gateway Logs:**

**Gateway 4 logs:**
```
numGateways: 2 (increased from 1!)
gatewayLoads[0]: {address: 0x0004, packetCount: X}
gatewayLoads[1]: {address: 0x0005, packetCount: Y}
Gateway bias calculation: Non-zero values
```

**Gateway 5 logs:**
```
numGateways: 2
Gateway load tracking active
Packets distributed between gateways
```

**Evidence W5 is Active:**
- Search logs for: "numGateways" should show 2
- Search logs for: "gatewayLoads" should show both gateways
- Search logs for: "bias" calculation with non-zero values

**PDR:** Aim for >90% (sensors send to either gateway)

---

### Expected Results

**Gateway Load Distribution:**
- Gateway 4: ~X packets
- Gateway 5: ~Y packets
- May not be perfectly balanced (sensors choose based on discovery)

**W5 Gateway Bias:**
- Should calculate relative load: (thisLoad - avgLoad) / avgLoad
- Not 0.0 (proves multi-gateway support)

**Validation:** Multi-gateway infrastructure works as designed

---

## Testing Schedule

**Day 1 (Today if possible):**
- Morning: Multi-hop test (1.5 hours)
  - Flash with 6 dBm
  - Position nodes physically
  - Test all 3 protocols
  - Verify hops>1

- Afternoon: Multi-gateway test (1 hour)
  - Modify config for Node 4 as gateway
  - Flash Protocol 3 only
  - Monitor both gateways
  - Verify W5 bias active

**Total:** 2.5-3 hours

**After Tests:**
- Analyze results (I'll help)
- Create ANALYSIS.md for critical tests
- Update thesis claims with multi-hop and multi-gateway evidence

---

## What These Tests Provide

**Multi-Hop Test:**
- Validates "mesh network" claim
- Shows relay necessity
- Demonstrates cost-based routing with observable differences
- Completes Protocol 3 validation

**Multi-Gateway Test:**
- Validates "gateway-aware" claim
- Activates W5 gateway bias feature
- Proves multi-gateway support
- Fulfills proposal promise

**Combined:** Address all critical thesis defense challenges

---

## Quick Reference Commands

**Multi-Hop Test (3-node, 6 dBm):**
```bash
# After editing power to 6 dBm
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 5 /dev/cu.usbserial-7

# Position 10-15m apart, then:
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --node2-port /dev/cu.usbserial-5 \
  --duration 1800 \
  --protocol protocol3 \
  --test-name "3node_physical_multihop_6dBm"
```

**Multi-Gateway Test (5-node, Node 4 as gateway):**
```bash
# After editing config.h for Node 4 gateway
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 4 /dev/cu.usbserial-6 && \
./flash_node.sh 5 /dev/cu.usbserial-7

python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol3 \
  --test-name "5node_multigateway"
```

---

**Ready to create detailed step-by-step guides?**
