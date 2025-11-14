# 4-Node Topology Testing Guide

**Purpose**: Validate Protocol 1, 2, and 3 with 4-node topologies to demonstrate scalability and multi-hop routing
**Duration**: 30 minutes per test
**Monitoring**: Gateway-only (Node 5 via USB serial)
**Date**: November 11, 2025
**Branch**: xMESH-Test

---

## Overview

### Why 4-Node Testing?

**Research Objectives:**
- Demonstrate scalability (3-node → 4-node validation)
- Validate multi-hop routing with intermediate nodes
- Compare Protocol 3 cost-based selection vs Protocol 2 hop-count
- Provide evidence for "network efficiency improves with intelligent routing"

**Test Approach:**
- **Monitoring**: Gateway node only (Node 5 via USB)
- **Other Nodes**: Battery powered (Nodes 1, 2, 3 or 4)
- **Metrics Captured**: PDR, HELLO overhead, routing table, latency

**Status (Nov 13, 2025):** All 4-node tests listed here were executed Nov 11 (see `experiments/results/protocol*/4node_*`). Keep this guide for future replication or if additional logs are required; no firmware changes are pending for these scenarios.

---

## Test Matrix

| Test Name | Topology | Nodes | Duration | Protocols | Priority |
|-----------|----------|-------|----------|-----------|----------|
| **4node_linear** | Linear chain | S1-S2-R3-G5 | 30 min | All 3 | ⭐⭐⭐ CRITICAL |
| **4node_diamond** | Diamond mesh | S1-R3/R4-G5 | 30 min | P2, P3 | ⭐⭐ RECOMMENDED |

**Total Time**: 4.5 hours (3 linear + 1.5 diamond)

---

## Topology 1: Linear 4-Node ⭐⭐⭐ CRITICAL

### Physical Configuration

```
[Sensor1]----[Sensor2]----[Relay3]----[Gateway5]
  Node 1       Node 2       Node 3       Node 5
```

**Node Assignments:**
| Node | ID | Role | Address | Serial Port | Power | Monitoring |
|------|-----|------|---------|-------------|-------|------------|
| 1 | 1 | SENSOR | 0x0001 | /dev/cu.usbserial-0001 | Battery | ❌ No |
| 2 | 2 | SENSOR | 0x0002 | /dev/cu.usbserial-0002 | Battery | ❌ No |
| 3 | 3 | RELAY | 0x0003 | /dev/cu.usbserial-5 | Battery | ❌ No |
| 5 | 5 | GATEWAY | 0x0005 | /dev/cu.usbserial-7 | **USB** | ✅ **YES** |

**Physical Placement:**
- All nodes on table within 1-2 meters
- Linear arrangement (not critical for spacing due to proximity)

**Expected Behavior:**
- Sensor1 & Sensor2: Generate data packets every 60s
- Relay3: Forward packets from both sensors
- Gateway5: Receive all packets (PDR target: >95%)

---

### Test 1.1: Protocol 1 (Flooding) - Linear 4-Node

**Test Name**: `4node_linear_protocol1_YYYYMMDD_HHMMSS`

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/1_flooding

# Flash all 4 nodes (TX power: 10 dBm)
./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Verify After Flashing:**
- Node 1 OLED: "SENSOR Node: 1"
- Node 2 OLED: "SENSOR Node: 2"
- Node 3 OLED: "RELAY Node: 3"
- Node 5 OLED: "GATEWAY Node: 5"
- All show: "Power: 10 dBm"

**Disconnect Nodes 1-3** (battery power only), **keep Node 5 on USB**

**Run Test:**
```bash
cd /Volumes/xMESH/xMESH-1

python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol1 \
  --test-name "4node_linear"
```

**Expected Results:**
- PDR: 100% (all packets from S1 and S2 reach G5)
- Relay forwarding: Both sensors' packets forwarded
- Total packets: ~60 (2 sensors × 30 packets each)

**Success Criteria:**
- ✅ PDR ≥ 95%
- ✅ Gateway receives packets from BOTH sensors
- ✅ Sequence numbers continuous (no gaps)

---

### Test 1.2: Protocol 2 (Hop-Count) - Linear 4-Node

**Test Name**: `4node_linear_protocol2_YYYYMMDD_HHMMSS`

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/2_hopcount

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol2 \
  --test-name "4node_linear"
```

**Expected Results:**
- PDR: 100%
- HELLO baseline: 60 HELLOs total (4 nodes × 15 HELLOs each)
- Routing table: 3 entries (sees all other nodes)

**Success Criteria:**
- ✅ PDR ≥ 95%
- ✅ Routing table converges within 60s
- ✅ ~60 HELLOs total (baseline for Protocol 3 comparison)

---

### Test 1.3: Protocol 3 (Gateway-Aware) - Linear 4-Node

**Test Name**: `4node_linear_protocol3_YYYYMMDD_HHMMSS`

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/3_gateway_routing

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol3 \
  --test-name "4node_linear"
```

**Expected Results:**
- PDR: 100%
- HELLO overhead: ~40 total (vs 60 baseline = 33% reduction)
- Trickle: I_max=600s reached
- Suppression: >70% efficiency
- False faults: 0

**Success Criteria:**
- ✅ PDR ≥ 95%
- ✅ HELLO reduction vs Protocol 2 (>20%)
- ✅ No false fault detections
- ✅ Trickle reaches I_max=600s

---

## Topology 2: Diamond 4-Node ⭐⭐ RECOMMENDED

### Physical Configuration

```
        [Relay3]
       /        \
      /          \
[Sensor1]      [Gateway5]
      \          /
       \        /
        [Relay4]
```

**Node Assignments:**
| Node | ID | Role | Address | Serial Port | Monitoring |
|------|-----|------|---------|-------------|------------|
| 1 | 1 | SENSOR | 0x0001 | /dev/cu.usbserial-0001 | ❌ No |
| 3 | 3 | RELAY | 0x0003 | /dev/cu.usbserial-5 | ❌ No |
| 4 | 4 | RELAY | 0x0004 | /dev/cu.usbserial-6 | ❌ No |
| 5 | 5 | GATEWAY | 0x0005 | /dev/cu.usbserial-7 | ✅ **YES** |

**Physical Placement:**
- Diamond layout on table (all within 2m)

**Expected Behavior:**
- Sensor1 has TWO paths to Gateway5:
  - Path A: S1 → R3 → G5
  - Path B: S1 → R4 → G5
- **Protocol 2**: Chooses based on hop count (both equal)
- **Protocol 3**: Chooses based on cost (signal quality matters!)

**Key Difference to Validate:**
- Protocol 2: Random or first-discovered path
- Protocol 3: **Cost-optimized path** (demonstrates multi-metric advantage!)

---

### Test 2.1: Protocol 2 (Hop-Count) - Diamond 4-Node

**Test Name**: `4node_diamond_protocol2_YYYYMMDD_HHMMSS`

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/2_hopcount

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 4 /dev/cu.usbserial-6
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol2 \
  --test-name "4node_diamond"
```

**Expected:**
- Gateway routing table shows path via R3 OR R4 (hop-count doesn't differentiate)
- PDR: 100%
- ~60 HELLOs total

---

### Test 2.2: Protocol 3 (Gateway-Aware) - Diamond 4-Node

**Test Name**: `4node_diamond_protocol3_YYYYMMDD_HHMMSS`

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/3_gateway_routing

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 4 /dev/cu.usbserial-6
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol3 \
  --test-name "4node_diamond"
```

**Expected:**
- Gateway routing table shows **cost-based path selection**
- Lower-cost path preferred (better RSSI/SNR/ETX)
- PDR: 100%
- ~40 HELLOs total (33% reduction)

**Key Validation:**
- Watch gateway logs for cost calculations
- Route should select based on link quality, not just hop count
- **This demonstrates Protocol 3's multi-metric advantage!**

---

## Data Collection & Analysis Workflow

### During Test (30 minutes)

**What to Watch** (Gateway serial output):
- Packets arriving from sensors (count for PDR)
- HELLO packets from all neighbors (count for overhead)
- Routing table updates (convergence time)
- Any errors or warnings

**Live Monitoring Checklist:**
- [ ] Gateway shows all 3 neighbors in routing table
- [ ] Packets arriving from both/all sensors
- [ ] No "Duty cycle" warnings
- [ ] No communication failures

---

### After Test Completion

**Automatic Output:**
- Test folder: `experiments/results/<protocol>/4node_<topology>_YYYYMMDD_HHMMSS/`
- Gateway log: `node1_YYYYMMDD_HHMMSS.log` (actually Node 5, but captured as node1)

**Manual Analysis Request:**
```
"Test complete. Please analyze and check for bugs.

experiments/results/<protocol>/4node_<topology>_YYYYMMDD_HHMMSS/"
```

**I Will Provide:**
- PDR calculation
- HELLO overhead count
- Routing table analysis
- Bug detection
- Comparison with 3-node baseline
- **ANALYSIS.md** document

---

## Analysis Checklist (What I'll Check)

### For Each Test:

**1. PDR Calculation:**
```bash
# Count packets from each sensor at gateway
Sensor 1 packets: grep "From.*0001\|src.*0001" | count
Sensor 2 packets: grep "From.*0002\|src.*0002" | count
Total expected: ~30 per sensor × 2 = 60 packets
PDR = (Received / Expected) × 100%
```

**2. HELLO Overhead:**
```bash
# Count HELLO packets received at gateway
grep -i "hello\|HELLO" | count
Protocol 2 baseline: 60 HELLOs (4 nodes × 15 each)
Protocol 3 target: <45 HELLOs (25-33% reduction)
```

**3. Routing Table Convergence:**
- Time to discover all 3 neighbors
- Stability (no flapping)
- Correct hop counts or costs

**4. Multi-Hop Validation:**
- Verify relay is in data path (not direct sensor→gateway)
- Check packet timing (sensor→relay→gateway delays)
- Validate 2-hop or 3-hop paths

**5. Protocol 3 Specific:**
- Trickle progression (60→120→240→480→600s)
- Suppression efficiency (>70% target)
- No false fault detections
- Cost-based route selection (if diamond topology)

---

## Success Criteria Summary

### Protocol 1 (Flooding):
- [ ] PDR ≥ 95%
- [ ] All sensor packets reach gateway
- [ ] Relay forwarding active
- [ ] Controlled flooding (no exponential explosion)

### Protocol 2 (Hop-Count):
- [ ] PDR ≥ 95%
- [ ] Routing table: 3 entries (all neighbors)
- [ ] HELLOs: ~60 total (baseline for comparison)
- [ ] Routes converge <60s

### Protocol 3 (Gateway-Aware):
- [ ] PDR ≥ 95%
- [ ] HELLO reduction: >20% vs Protocol 2
- [ ] Trickle reaches I_max=600s
- [ ] Suppression efficiency >70%
- [ ] No false faults
- [ ] Cost-based selection (diamond topology)

---

## Quick Reference Commands

### Flash 4-Node Linear Setup

**Protocol 1:**
```bash
cd firmware/1_flooding
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Protocol 2:**
```bash
cd firmware/2_hopcount
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Protocol 3:**
```bash
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 5 /dev/cu.usbserial-7
```

### Run 30-Minute Test (Gateway Monitoring Only)

```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol <protocol1|protocol2|protocol3> \
  --test-name "4node_linear"
```

**Note:** Script parameter is `--node1-port` but actually captures Node 5 (gateway)

---

## Flash 4-Node Diamond Setup

**Protocol 2 & 3 Only** (diamond topology tests route selection):

```bash
cd firmware/<protocol>

./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 4 /dev/cu.usbserial-6 && \
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Diamond Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol <protocol2|protocol3> \
  --test-name "4node_diamond"
```

---

## Troubleshooting

### If PDR < 95%

**Check:**
1. Are all nodes powered on? (check OLED displays)
2. Is gateway receiving ANY packets? (check serial output)
3. Are there duty cycle warnings?
4. Is frequency correct (923.2 MHz)?

**Actions:**
- Verify all nodes booted successfully (check OLEDs)
- Reduce PACKET_INTERVAL_MS if duty cycle warnings
- Check antennas are attached
- Power cycle all nodes and retry

### If Gateway Sees No Neighbors

**Check:**
1. Did HELLO discovery occur? (wait 2-3 minutes)
2. Are nodes within range? (10 dBm @ 1-2m should work)
3. Check gateway routing table size (should be 3 for 4-node)

**Actions:**
- Wait longer for HELLO convergence (up to 5 minutes)
- Verify all nodes show same frequency
- Check for "Route added" messages in gateway log

### If HELLO Count Seems Wrong

**Protocol 2 Expected:**
- 30 minutes ÷ 2 minutes = 15 HELLOs per node
- 4 nodes × 15 = 60 HELLOs total

**Protocol 3 Expected:**
- Trickle adaptive: 5-10 Trickle HELLOs
- Safety HELLOs: 30-35 (180s interval)
- Total: 35-45 HELLOs
- Reduction: 25-40% vs Protocol 2

---

## Post-Test Analysis Request Template

**After each test completes, send me:**

```
Test <X> complete. Please analyze and check for bugs.

experiments/results/<protocol>/4node_<topology>_YYYYMMDD_HHMMSS/
```

**I will provide:**
- ✅ PDR calculation with breakdown
- ✅ HELLO overhead analysis
- ✅ Routing convergence timing
- ✅ Bug detection and issues
- ✅ Comparison with 3-node baseline
- ✅ Scalability metrics
- ✅ **ANALYSIS.md** document in test folder

---

## Testing Schedule Recommendation

**Day 1 (Today):**
- Test 1.1: Protocol 1 - Linear 4-node (30 min)
- Test 1.2: Protocol 2 - Linear 4-node (30 min)
- Test 1.3: Protocol 3 - Linear 4-node (30 min)
- **Total**: 1.5 hours

**Day 2 (Optional):**
- Test 2.1: Protocol 2 - Diamond 4-node (30 min)
- Test 2.2: Protocol 3 - Diamond 4-node (30 min)
- **Total**: 1 hour

**Analysis Time**: ~30 min per test (I'll do this for you)

---

## Gateway Monitoring - What Gets Captured

### Complete PDR Data ✅

**Gateway Log Contains:**
```
[GATEWAY] Packet X from 0001 received (hops=2, value=42.5)
[GATEWAY] Packet Y from 0002 received (hops=1, value=38.2)
```

**Analysis:**
- Count packets from each sensor source
- Verify sequence numbers (detect gaps = packet loss)
- **PDR = (Gateway RX) / (Expected TX)**
- Expected TX: 30 min ÷ 60s = 30 packets per sensor

### HELLO Overhead ✅

**Gateway Log Contains:**
```
[HELLO] Received from 0001
[HELLO] Received from 0002
[HELLO] Received from 0003
```

**Analysis:**
- Count total HELLO receptions
- Compare Protocol 2 (baseline) vs Protocol 3 (reduced)
- **Overhead % = (P2 HELLOs - P3 HELLOs) / P2 HELLOs**

### Routing Table Evolution ✅

**Gateway Log Contains:**
```
Routing table size: 0
Routing table size: 1 (discovered Node 3)
Routing table size: 2 (discovered Node 1)
Routing table size: 3 (discovered Node 2)
```

**Analysis:**
- Convergence time (startup to full table)
- Stability (no route flapping)
- Correct hop counts/costs

---

## Ready to Start Testing! 🚀

**Next Steps:**
1. Flash 4 nodes with Protocol 1 (linear topology)
2. Run 30-minute test (monitor gateway only)
3. Send me the test folder path for analysis
4. Repeat for Protocol 2, then Protocol 3
5. (Optional) Diamond topology tests

**Each test provides complete data** despite monitoring only the gateway!
