# 5-Node Topology Testing Guide

**Purpose**: Final scalability validation with 5-node deployments
**Duration**: 30 minutes per test
**Monitoring**: Gateway + 1-2 other nodes (relay or sensor)
**Date**: November 11, 2025
**Branch**: xMESH-Test

**Based on 4-Node Results:**
- Protocol 3 achieves 96.7-100% PDR (superior to 81-85% baselines)
- Overhead reduction: 27% validated
- Ready for 5-node scalability proof

**Status (Nov 13, 2025):** Both `5node_linear_stable_20251112_024908` and `5node_linear_stable_after-10min-one-sensor-down_20251112_032513` are complete (100% PDR, 27% HELLO reduction, and clean fault detection). Leave this guide in place for reruns or outdoor repeats; no firmware changes pending.

---

## Test Matrix - 5-Node Topologies

| Test | Topology | Nodes | Sensors | Duration | Protocols | Priority |
|------|----------|-------|---------|----------|-----------|----------|
| **5node_linear** | Linear chain | S1-S2-R3-R4-G5 | 2 | 30 min | All 3 | ⭐⭐⭐ CRITICAL |
| **5node_mesh** | Full mesh | S1,S2,R3,R4,G5 | 2 | 30 min | P2, P3 | ⭐⭐ RECOMMENDED |

**Total Time:** ~4.5 hours (3 linear + 1.5 mesh)

---

## Topology 1: Linear 5-Node (Maximum Scalability) ⭐⭐⭐

### Physical Configuration

```
[Sensor1]--[Sensor2]--[Relay3]--[Relay4]--[Gateway5]
  Node 1     Node 2     Node 3    Node 4     Node 5
```

**Maximum hops:** 4 (if forced multi-hop with power reduction)
**Current (10 dBm):** All likely direct paths (4m radius)

**Node Assignments:**
| Node | ID | Role | Address | Serial Port | Monitoring |
|------|-----|------|---------|-------------|------------|
| 1 | 1 | SENSOR | 0x0001 | /dev/cu.usbserial-0001 | ❌ Battery |
| 2 | 2 | SENSOR | 0x0002 | /dev/cu.usbserial-0002 | ❌ Battery |
| 3 | 3 | RELAY | 0x0003 | /dev/cu.usbserial-5 | ❌ Battery |
| 4 | 4 | RELAY | 0x0004 | /dev/cu.usbserial-6 | ✅ **USB** (optional) |
| 5 | 5 | GATEWAY | 0x0005 | /dev/cu.usbserial-7 | ✅ **USB** (required) |

**Monitoring Options:**
- **Minimum:** Gateway only (captures PDR, overhead)
- **Recommended:** Gateway + Relay4 (validates forwarding)
- **Maximum:** Gateway + Relay4 + Sensor1 (complete visibility)

---

### Test 1.1: Protocol 1 (Flooding) - Linear 5-Node

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/1_flooding

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 4 /dev/cu.usbserial-6
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Verify OLEDs:**
- Node 1: "SENSOR Node: 1"
- Node 2: "SENSOR Node: 2"
- Node 3: "RELAY Node: 3"
- Node 4: "RELAY Node: 4"
- Node 5: "GATEWAY Node: 5"

**Disconnect Nodes 1-3** (battery), **Monitor Nodes 4-5** (USB)

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol1 \
  --test-name "5node_linear"
```

**Expected Results (Based on 4-Node):**
- PDR: 75-85% (2 sensors, 5 nodes, more collision)
- Total packets: ~60 (2 sensors × 30 each)
- Relay4 forwarding: Active
- Gateway receives from both sensors

**Success Criteria:**
- ✅ Test completes without crashes
- ✅ Gateway receives packets from both sensors
- ✅ Relay4 shows forwarding activity
- ⚠️ PDR may be <95% (acceptable for baseline)

---

### Test 1.2: Protocol 2 (Hop-Count) - Linear 5-Node

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/2_hopcount

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 4 /dev/cu.usbserial-6
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol2 \
  --test-name "5node_linear"
```

**Expected Results:**
- PDR: 75-85% (similar to 4-node linear)
- HELLO baseline: ~75 HELLOs (5 nodes × 15 each)
- Routing table: 4 entries (all neighbors)
- Direct paths likely (10 dBm, 4m radius)

**Success Criteria:**
- ✅ Establishes 5-node HELLO baseline
- ✅ PDR measured for comparison
- ✅ Routing table converges

---

### Test 1.3: Protocol 3 (Gateway-Aware) - Linear 5-Node ⭐ CRITICAL

**Flash Sequence:**
```bash
cd /Volumes/xMESH/xMESH-1/firmware/3_gateway_routing

./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 4 /dev/cu.usbserial-6
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol3 \
  --test-name "5node_linear"
```

**Expected Results (Based on 4-Node Success):**
- **PDR: 90-100%** (should exceed or match baselines)
- **HELLO overhead: ~50-55** (vs 75 baseline = 27-33% reduction)
- Trickle: I_max=600s
- Suppression: >70%
- False faults: 0

**Critical Validation:**
- Does Protocol 3 maintain >95% PDR with 5 nodes?
- Does it outperform baselines (like 4-node did)?
- Does overhead reduction scale (27-33%)?

---

## Topology 2: Mesh 5-Node (Full Connectivity) ⭐⭐

### Physical Configuration

```
      [Sensor2]
     /    |    \
[Sensor1]-[Relay3]-[Gateway5]
     \    |    /
      [Relay4]
```

**All nodes interconnected** (at 4m radius, 10 dBm = all direct)

**Expected:** High redundancy, multiple path options

**Node Assignments:**
| Node | ID | Role | Monitoring |
|------|-----|------|------------|
| 1 | 1 | SENSOR | ❌ Battery |
| 2 | 2 | SENSOR | ❌ Battery or ✅ USB |
| 3 | 3 | RELAY | ❌ Battery |
| 4 | 4 | RELAY | ✅ **USB** |
| 5 | 5 | GATEWAY | ✅ **USB** |

**Monitoring:** Gateway + Relay4 (or Gateway + Sensor2 for TX verification)

---

### Test 2.1: Protocol 2 (Hop-Count) - Mesh 5-Node

**Flash Sequence:**
```bash
cd firmware/2_hopcount

./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 4 /dev/cu.usbserial-6 && \
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test:**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol protocol2 \
  --test-name "5node_mesh"
```

**Expected:** 75-85% PDR, ~75 HELLOs

---

### Test 2.2: Protocol 3 (Gateway-Aware) - Mesh 5-Node

**Flash & Run:**
```bash
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
  --test-name "5node_mesh"
```

**Expected:** 90-100% PDR, ~50-55 HELLOs (27-33% reduction)

---

## Expected Results Based on 4-Node Patterns

### PDR Predictions

| Topology | Sensors | Protocol 1 | Protocol 2 | **Protocol 3** | P3 Expected Advantage |
|----------|---------|-----------|-----------|----------------|---------------------|
| **5-Node Linear** | 2 | 75-85% | 75-85% | **90-100%** | **+10-15%** |
| **5-Node Mesh** | 2 | 75-85% | 75-85% | **90-100%** | **+10-15%** |

**Based on:** Protocol 3 exceeded baselines by 11.7-15% in 4-node linear test

---

### HELLO Overhead Predictions

**Protocol 2 Baseline (5 nodes, 30 min):**
- 5 nodes × 15 HELLOs (120s interval) = **75 HELLOs**

**Protocol 3 Expected:**
- Trickle + Safety: ~55 HELLOs
- Reduction: (75-55)/75 = **27-33%**

**Same efficiency** as 3-node and 4-node tests

---

## Monitoring Strategy for 5-Node

### Option A: Gateway + Relay4 (Recommended)

**Captures:**
- ✅ PDR (gateway receives all)
- ✅ Relay forwarding (Relay4 activity)
- ✅ HELLO overhead (both perspectives)
- ⚠️ Missing: Sensor TX confirmation

**Ports:**
- node1-port: /dev/cu.usbserial-6 (Relay4)
- node2-port: /dev/cu.usbserial-7 (Gateway)

---

### Option B: Gateway + Sensor2 (TX Verification)

**Captures:**
- ✅ PDR (gateway receives)
- ✅ Sensor TX (confirms packet generation)
- ✅ Sensor routing behavior
- ⚠️ Missing: Relay forwarding stats

**Ports:**
- node1-port: /dev/cu.usbserial-0002 (Sensor2)
- node2-port: /dev/cu.usbserial-7 (Gateway)

---

### Option C: Gateway Only (Minimal)

**Captures:**
- ✅ PDR calculation
- ✅ HELLO overhead
- ✅ Routing table
- ⚠️ Missing: Intermediate node details

**Ports:**
- node1-port: /dev/cu.usbserial-7 (Gateway only)

---

## Quick Reference - 5-Node Linear

**Flash All Protocols:**
```bash
# Protocol 1
cd firmware/1_flooding
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 4 /dev/cu.usbserial-6 && \
./flash_node.sh 5 /dev/cu.usbserial-7

# Protocol 2
cd ../2_hopcount
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 4 /dev/cu.usbserial-6 && \
./flash_node.sh 5 /dev/cu.usbserial-7

# Protocol 3
cd ../3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001 && \
./flash_node.sh 2 /dev/cu.usbserial-0002 && \
./flash_node.sh 3 /dev/cu.usbserial-5 && \
./flash_node.sh 4 /dev/cu.usbserial-6 && \
./flash_node.sh 5 /dev/cu.usbserial-7
```

**Run Test (Gateway + Relay4):**
```bash
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-6 \
  --node2-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --protocol <protocol1|protocol2|protocol3> \
  --test-name "5node_linear"
```

---

## Success Criteria

### Protocol 1 (Flooding):
- [ ] Test completes without memory issues
- [ ] Gateway receives from both sensors
- [ ] Relay4 forwards packets
- [ ] PDR: Any (baseline reference)

### Protocol 2 (Hop-Count):
- [ ] PDR: Any (baseline reference)
- [ ] HELLO baseline: ~75 total
- [ ] Routing table: 4 entries
- [ ] Routes converge <60s

### Protocol 3 (Gateway-Aware) - CRITICAL:
- [ ] **PDR: ≥90%** (maintain superiority)
- [ ] **HELLO reduction: >20%** vs Protocol 2
- [ ] Trickle reaches I_max=600s
- [ ] Suppression >70%
- [ ] No false faults
- [ ] **Exceeds or matches baselines** (like 4-node)

---

## Predicted Results (Based on 4-Node Patterns)

### Collision Impact with 5 Nodes

**4-Node Linear (2 Sensors):**
- P1: 85.0%, P2: 81.7%, P3: 96.7%

**5-Node Linear (2 Sensors):**
- **Predicted P1:** 75-85% (more nodes = more collision)
- **Predicted P2:** 75-85% (similar to P1)
- **Predicted P3:** **85-95%** (reduced overhead helps, should exceed baselines)

**Key Question:** Does Protocol 3 maintain >90% PDR with 5 nodes?

---

## Analysis Promise

**After each 5-node test:**
```
Test complete. Please analyze:
experiments/results/<protocol>/5node_<topology>_YYYYMMDD_HHMMSS/
```

**I will provide:**
- PDR calculation (per sensor and combined)
- Comparison with 3-node and 4-node baselines
- Scalability analysis (performance vs network size)
- HELLO overhead validation
- Memory/resource usage assessment
- Complete ANALYSIS.md document

---

## Scalability Metrics to Track

### Network Size Comparison

| Nodes | P1 PDR | P2 PDR | **P3 PDR** | P3 Advantage |
|-------|--------|--------|-----------|--------------|
| 3 | 100% | 100% | 100% | Baseline |
| 4 | 85% | 81.7% | **96.7%** | **+11.7-15%** |
| 5 | ? | ? | **?** | **Target: +10%** |

**Thesis Question:** Does Protocol 3 advantage persist or grow with scale?

---

### Overhead Scaling

| Nodes | P2 HELLOs | P3 HELLOs | Reduction | Status |
|-------|-----------|-----------|-----------|--------|
| 3 | 45 | 31 | 31% | ✅ Validated |
| 4 | 60 | 44 | 27% | ✅ Validated |
| 5 | 75 | **~55** | **27-33%** | **Target** |

**Thesis Question:** Does overhead reduction maintain with scale?

---

## Troubleshooting

### If PDR Very Low (<70%)

**Possible Causes:**
- Too many nodes transmitting (collision)
- 10 dBm too high (interference)
- Memory issues (5 nodes = more routing table entries)

**Solutions:**
- Reduce TX power to 6-8 dBm
- Increase packet interval (reduce traffic)
- Check memory usage in logs

### If Gateway Routing Table Incomplete

**Check:**
- Wait 3-5 minutes for HELLO convergence
- Verify all nodes powered on
- Check OLED displays for boot status

**Expected:** Gateway routing table should have 4 entries (all other nodes)

---

## Memory Validation (From Scalability Assessment)

**3-Node Usage:** 22,304 bytes (6.8%)
**4-Node Estimated:** 22,400 bytes (6.8%)
**5-Node Estimated:** 22,500 bytes (6.9%)

**Expected:** No memory issues, <7% RAM usage

**Verify in logs:** Check "Memory: X KB free" in heartbeat messages

---

## Ready to Start! 🚀

**Recommended Testing Order:**
1. Protocol 1 - Linear 5-node (30 min) - Baseline
2. Protocol 2 - Linear 5-node (30 min) - Baseline
3. **Protocol 3 - Linear 5-node** (30 min) - **Critical comparison**
4. (Optional) Protocol 2 - Mesh 5-node
5. (Optional) Protocol 3 - Mesh 5-node

**Minimum for thesis:** Linear 5-node (3 tests, 1.5 hours)

**After each test, send me the folder path for analysis!**

---

**Start with Protocol 1 linear 5-node when ready!**
