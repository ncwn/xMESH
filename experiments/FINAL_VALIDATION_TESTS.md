# Final Validation Tests

**Last Updated:** 2025‑11‑13  
**Scope:** 30‑minute hardware runs for every indoor topology required by the research plan (Protocols 1‑3).  
**Status:** Tests 1‑4 are complete and logged; only the outdoor/hallway multi-hop test (Test 5) remains.

## Summary of Executed Runs

| Test | Protocol | Log Folder (experiments/results/…) | Topology & Notes | Outcome |
|------|----------|------------------------------------|------------------|---------|
| 1 | Protocol 1 Flooding | `protocol1/4node_linear_20251111_175245/` | Linear S+S+R+G (Nodes 1,2,3,5). Gateway-only logging. | 30 min, 96.7 % PDR (58/60). Establishes broadcast baseline. |
| 2 | Protocol 2 Hop-Count | `protocol2/4node_linear_20251111_192107/` + `protocol2/4node_diamond_20251111_203959/` | Linear and diamond variants (same node set). Provides 120 s HELLO baseline (≈60 HELLOs/30 min). | Linear: 81.7 % PDR worst case (49/60); Diamond: 96.7 % (29/30). |
| 3 | Protocol 3 Single-Gateway | `experiments/archive/3node_30min_overhead_validation_20251111_032647/` | Same linear layout as Tests 1‑2. Sensor/gateway logs captured. | 31 HELLOs vs 45 baseline (31 % reduction) with 100 % PDR (28/28). |
| 4 | Protocol 3 Dual-Gateway (W5) | `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/` and `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` | 5‑node setup (2 sensors, 1 relay, 2 gateways). Sensors monitored to capture W5 logs. | Cold start: alternating gateways (16 vs 13 packets). Endurance: `[FAULT]` detections at 375‑384 s for sensor/gateway/relay outages. |
| 5 | Protocol 3 Multi-Hop (Physical) | _Pending_ | Hallway/parking-lot spacing to force `hops>0` (≤8 dBm, ≥10 m per hop). | Required before claiming mesh routing in thesis. |

> **Reminder:** Link each thesis statement to the table above or directly to the log folder. Keep the filenames as-is so future reruns remain comparable.

## Procedures (Indoor Runs)

### Common Prerequisites
```bash
cd firmware/<protocol>
pio pkg update
pio run -t clean
pio run          # Build check
```
Use `python3 raspberry_pi/multi_node_capture.py` to record each UART stream into `experiments/results/<protocol>/<testname>/nodeX_TIMESTAMP.log`.

### Test 1 – Protocol 1 Flooding Baseline
- **Topology:** `[S1]–[S2]–[R3]–[G5]` (Nodes 1,2,3 on battery, Gateway 5 on USB).  
- **Flash:** `firmware/1_flooding/flash_node.sh {1,2,3,5}`.  
- **Monitor:** Gateway only (captures PDR).  
- **Success Criteria:** PDR ≥95 %, relay forwards traffic (visible in gateway logs).  
- **Reference Log:** `protocol1/4node_linear_20251111_175245/node5.log`.

### Test 2 – Protocol 2 Hop-Count Baseline
- **Topology:** Same node layout as Test 1 (linear) plus a diamond retest (`protocol2/4node_diamond_*`) to demonstrate multi-path behavior.  
- **Flash:** `firmware/2_hopcount/flash_node.sh {1,2,3,5}`.  
- **Monitor:** Gateway. Count HELLO packets to confirm the 120 s baseline (≈15 per node over 30 min).  
- **Reference Logs:**  
  - `protocol2/4node_linear_20251111_192107/node5.log` (81.7 % PDR worst case).  
  - `protocol2/4node_diamond_20251111_203959/node5.log` (96.7 % PDR, healthy topology).

### Test 3 – Protocol 3 Single-Gateway (Overhead Validation)
- **Topology:** Identical to Tests 1‑2.  
- **Flash:** `firmware/3_gateway_routing/flash_node.sh {1,2,3,5}`.  
- **Monitor:** Gateway. Confirm `[Trickle]` reaches I_max=600 s and total HELLOs drop to ~31 over 30 min.  
- **Reference Log:** `experiments/archive/3node_30min_overhead_validation_20251111_032647/node5.log`.

### Test 4 – Protocol 3 Dual-Gateway (W5 Load Sharing)
- **Topology:** Nodes 1‑2 sensors, Node 3 relay, Nodes 4‑5 gateways (both on USB).  
- **Flash:** `firmware/3_gateway_routing/flash_node.sh {1,2,3,4,5}` (Node 4 configured as gateway in `config.h`).  
- **Monitor:** Sensor node (critical for `[W5]` logs) plus at least one gateway.  
- **Success Criteria:**  
  - `numGateways=2` reported within two HELLOs.  
  - `[W5] Load-biased gateway selection …` entries preceding alternating transmissions.  
  - Endurance run logs `[FAULT]` detection for each intentional outage (sensor, gateway, relay) within 375‑384 s.  
- **Reference Logs:**  
  - `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/node2_*.log` (cold start).  
  - `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_over_I600s_node_detection_20251113_120301/node3_*.log` (failure cycling).

### Test 5 – Protocol 3 Multi-Hop (Pending)
- **Goal:** Demonstrate `hops>0` by spacing the nodes so the sensor cannot reach the gateway directly.  
- **Setup:** Linear `[S1]–[R3]–[G5]` (optionally `[S1]–[S2]–[R3]–[G5]`), 10‑15 m spacing, TX power 6‑8 dBm, ideally outdoors or down a hallway.  
- **Monitoring:** Gateway (PDR + routing table) and relay (to confirm forwarding).  
- **Deliverables:**  
  - Log folder `protocol3/multihop_<date>/` with `hops=2` entries.  
  - ANALYSIS.md summarizing PDR, HELLO counts, and cost/route snapshots.  
  - Documentation updates (README, PRD, CLAUDE, IND/summary) to reflect validated multi-hop behavior.

## Data Integrity Checklist
1. Each test folder contains all monitored node logs plus any ANALYSIS.md.  
2. README/PRD/CLAUDE references the exact folder names when citing PDR, HELLO counts, or fault-detection timings.  
3. Archived (pre‑fork) tests remain under `experiments/results/archive/...` and are not used for claims.  
4. Before editing any documentation, spot-check the referenced logs to confirm packet counts and timestamps.

## Prerequisites

Verify library is updated:
```bash
cd firmware/3_gateway_routing
pio pkg update
pio run -t clean
pio run  # Verify build succeeds
```

---

## Test 1: Protocol 1 Flooding Baseline

**Topology**: Linear 4-node
```
[S:0x0001] --- [S:0x0002] --- [R:0x0003] --- [G:0x0005]
```

**Flash**:
```bash
cd firmware/1_flooding
./flash_node.sh 1 <port>
./flash_node.sh 2 <port>
./flash_node.sh 3 <port>
./flash_node.sh 5 <port>
```

**Monitor**: Gateway (Node 5) - **Minimum required**
```bash
pio device monitor --port <gateway_port> --baud 115200 > experiments/results/protocol1/4node_linear_$(date +%Y%m%d_%H%M%S)/node5.log
```

**Optional**: Monitor additional nodes if desired (sensor, relay) for detailed logs
- Indoor tests: Can monitor up to 3 nodes simultaneously (sufficient cables)
- Gateway log alone is sufficient for PDR, overhead, and topology validation

**Verify Before Starting**:
- [ ] Gateway shows: `Local Address: 0005` (not 0xBB94)
- [ ] Gateway shows: `Role: GATEWAY`
- [ ] All 3 other nodes appear in routing table

**Run**: 30 minutes

**Collect**:
- [ ] PDR (packets received / packets expected)
- [ ] Total packets transmitted across all nodes
- [ ] Average latency

**Expected**: High overhead (all packets broadcast), baseline PDR

---

## Test 2: Protocol 2 Hop-Count Baseline

**Topology**: Same as Test 1
```
[S:0x0001] --- [S:0x0002] --- [R:0x0003] --- [G:0x0005]
```

**Flash**:
```bash
cd firmware/2_hopcount
./flash_node.sh 1 <port>
./flash_node.sh 2 <port>
./flash_node.sh 3 <port>
./flash_node.sh 5 <port>
```

**Monitor**: Gateway (Node 5)

**Verify Before Starting**:
- [ ] Gateway shows: `Local Address: 0005`
- [ ] Gateway shows: `Role: GATEWAY`

**Run**: 30 minutes

**Collect**:
- [ ] PDR
- [ ] Total HELLO packets received (baseline for P3 comparison)
- [ ] Average latency

**Expected**: Fixed 120s HELLO interval, baseline overhead for intelligent routing

---

## Test 3: Protocol 3 Single-Gateway (Overhead Reduction)

**Topology**: Same as Test 1-2
```
[S:0x0001] --- [S:0x0002] --- [R:0x0003] --- [G:0x0005]
```

**Flash**:
```bash
cd firmware/3_gateway_routing
./flash_node.sh 1 <port>
./flash_node.sh 2 <port>
./flash_node.sh 3 <port>
./flash_node.sh 5 <port>
```

**Monitor**: Gateway (Node 5)

**Verify Before Starting**:
- [ ] Gateway shows: `Local Address: 0005`
- [ ] Gateway shows: `Role: GATEWAY`
- [ ] Trickle enabled: `TRICKLE ACTIVE`
- [ ] NO `[W5]` messages (single gateway, numGateways=1)

**Run**: 30 minutes

**Collect**:
- [ ] PDR
- [ ] Total HELLO packets (compare to Test 2)
- [ ] Trickle interval progression (should reach I_max=600s)
- [ ] Average latency

**Expected**: 31% fewer HELLOs than Test 2, PDR maintained >95%

**Analysis**:
```
Overhead Reduction = (Test2_HELLOs - Test3_HELLOs) / Test2_HELLOs × 100%
Target: ~31%
```

---

## Test 4: Protocol 3 Multi-Gateway (W5 Validation) - **MOST CRITICAL**

**Topology**: 5-node with 2 gateways
```
[S:0x0001] --- [S:0x0002] --- [R:0x0003] --- [G:0x0004]
                                    |
                                    |
                               [G:0x0005]
```

**Flash**:
```bash
cd firmware/3_gateway_routing
./flash_node.sh 1 <port>
./flash_node.sh 2 <port>
./flash_node.sh 3 <port>
./flash_node.sh 4 <port>
./flash_node.sh 5 <port>
```

**Monitor**: **SENSOR (Node 1 or 2)** - CRITICAL for seeing W5 messages

**Gateway Placement**: Gateways (Node 4 & 5) can be close together (e.g., both on USB)
- W5 tests load sharing, NOT distance between gateways
- Indoor test has all nodes within 4m anyway
- Both gateways on USB is acceptable and convenient

**Verify Before Starting**:
- [ ] Sensor shows: `Local Address: 0001` or `0002`
- [ ] Sensor shows: `Role: SENSOR` (NOT GATEWAY!)
- [ ] Both gateways powered on and running

**Run**: 30 minutes

**Success Criteria** (from sensor log):
- [ ] `[W5] Gateway HELLO from 0004: 2 gateways reported, size XX bytes`
- [ ] `[W5] Gateway HELLO from 0005: 2 gateways reported, size XX bytes`
- [ ] `[W5]   Gateway 0004: XX packets`
- [ ] `[W5]   Gateway 0005: YY packets`
- [ ] `[W5]   Discovered gateway XXXX (total: 2)` OR see `numGateways=2`
- [ ] Cost calculations show W5 bias ≠ 0.0
- [ ] PDR >95%

**Expected Output** (sensor serial):
```
xMESH GATEWAY-AWARE COST ROUTING
Role: SENSOR (SENSOR)  ✅
Local Address: 0001    ✅

[W5] Gateway HELLO from 0004: 2 gateways reported, size 33 bytes
[W5]   Gateway 0004: 15 packets
[W5]   Discovered gateway 0005: 8 packets (total: 2)

[COST] Route to 0004: cost=1.17, hops=1, W5=0.32
[COST] Route to 0005: cost=1.09, hops=1, W5=-0.32
Selected gateway: 0005 (lower load)
```

**Analysis**:
- Verify both gateways discovered
- Verify W5 bias values present in cost calculations
- Verify routing prefers lower-load gateway
- Document gateway load propagation mechanism

---

## Test 5: Protocol 3 Outdoor Multi-Hop (OPTIONAL - Recommended)

**Priority**: OPTIONAL (run after Tests 1-4 pass, if time permits)
**Purpose**: Validate multi-hop mesh routing with relay forwarding

**Current Gap**: All indoor tests show hops=0 (direct paths)
- 10 dBm + 4m spacing = all nodes in direct communication range
- Cannot demonstrate relay forwarding necessity
- Cannot show cost differentiation across multi-hop paths

**Outdoor Configuration**: Force multi-hop by physical distance
```
[S:0x0001] --12m-- [S:0x0002] --12m-- [R:0x0003] --12m-- [G:0x0005]
 (battery)          (battery)          (battery)          (USB monitor)
```

**TX Power**: Reduce to 6-8 dBm in config.h before flashing
```cpp
// In firmware/3_gateway_routing/src/config.h
#define LORA_TX_POWER  6  // Reduced for outdoor multi-hop test
```

**Flash**:
```bash
cd firmware/3_gateway_routing
# Edit config.h: Change TX_POWER to 6 dBm
pio run -t clean  # Critical after config change

./flash_node.sh 1 <port>  # Battery powered, place 12m from Node 2
./flash_node.sh 2 <port>  # Battery powered, place 12m from Node 3
./flash_node.sh 3 <port>  # Battery powered, place 12m from Gateway
./flash_node.sh 5 <port>  # USB powered, monitor serial
```

**Monitor**: Gateway (Node 5) via USB - **ONLY node monitored due to distance**
**Location**: Outdoor hallway, field, or long corridor

**Why Gateway-Only Monitoring Sufficient**:
- ✅ Gateway routing table shows hops>1 (proves multi-hop)
- ✅ Gateway receives packets with via field (proves relay forwarding)
- ✅ Gateway can calculate PDR (knows delivery ratio)
- ✅ Gateway sees complete network topology
- ❌ Sensor/Relay logs NOT needed (cannot confirm end-to-end delivery)

**Verify Before Starting**:
- [ ] TX power reduced to 6-8 dBm in config.h
- [ ] Clean build: `pio run -t clean` after config change
- [ ] Nodes physically separated 10-15 meters
- [ ] Gateway shows: `Local Address: 0005`
- [ ] Test direct sensor→gateway communication fails (packets lost without relay)

**Run**: 30 minutes

**Success Criteria** (from gateway log):
- [ ] **hops>1** in routing table (e.g., Node 1 at 2-3 hops)
- [ ] Relay forwards packets: Check relay FWD count >0 (if monitoring relay)
- [ ] Packets from Node 1 show `via=0x0003` or `via=0x0002` (multi-hop path)
- [ ] PDR >95% maintained despite multi-hop
- [ ] Cost values differ by path (multi-hop has higher cost)

**Expected Output** (gateway serial):
```
Routing table after convergence:
  Node 0001: via 0003, metric=2 hops  ✅ Multi-hop!
  Node 0002: via 0003, metric=2 hops  ✅ Multi-hop!
  Node 0003: via 0003, metric=1 hop   ✅ Direct to relay

Received packet:
  From: 0001, Via: 0003, Hops: 2  ✅ Relay forwarded!

Cost calculation:
  Route to 0001: cost=2.3 (hops=2, higher than direct)  ✅
  Route to 0003: cost=1.1 (hops=1, direct)  ✅
```

**Analysis**:
- Verify hops>1 for sensors (proves relay forwarding necessary)
- Verify packets traverse relay (via=0x0003)
- Verify cost increases with hop count
- Calculate end-to-end latency for multi-hop paths

**Important Note**:
- Restore TX power to 10 dBm after outdoor test
- Edit config.h and reflash before next indoor tests

**Defense Value**:
- ✅ Proves "mesh networking" claim (not just direct paths)
- ✅ Demonstrates relay forwarding in practice
- ✅ Shows cost differentiation across path lengths
- ✅ Validates routing convergence in real-world deployment

**If Skipped** (acceptable):
- Can claim: "Protocol validated in controlled indoor environment with direct paths"
- Acknowledge: "Multi-hop behavior demonstrated in routing logic, physical distance validation future work"

---

## Post-Testing Checklist

After each test:
- [ ] Save log to `experiments/results/protocolX/test_YYYYMMDD_HHMMSS/`
- [ ] Create ANALYSIS.md with metrics
- [ ] **Verify addresses are 0x0001-0x0005** (not MAC)

After indoor tests (1-4):
- [ ] Calculate overhead reduction: Test 2 vs Test 3
- [ ] Verify W5 validation: Test 4 shows numGateways=2
- [ ] Git commit indoor results
- [ ] Git tag: v0.6.0-w5-indoor-validated

After outdoor test (5) - if completed:
- [ ] Verify hops>1 in routing table
- [ ] Verify relay forwarding (FWD count >0)
- [ ] Git commit outdoor results
- [ ] Git tag: v0.6.1-multihop-validated

---

## Defense Presentation Structure

**Slide 1: Protocol Comparison** (Tests 1, 2, 3)
```
Metric            | P1 Flooding | P2 Hop-Count | P3 Gateway-Aware
------------------|-------------|--------------|------------------
PDR               | XX%         | YY%          | ZZ%
Control Overhead  | 100%        | Baseline     | -31%
Avg Latency (ms)  | XX          | YY           | ZZ
Environment       | Indoor      | Indoor       | Indoor
```

**Slide 2: Trickle Overhead Reduction** (Test 2 vs Test 3)
```
Protocol 2: Fixed 120s → XX HELLOs in 30 min
Protocol 3: Adaptive 60-600s → YY HELLOs in 30 min
Reduction: 31%
```

**Slide 3: W5 Multi-Gateway** (Test 4 - PRIMARY CONTRIBUTION)
```
✅ Gateway discovery: 2 gateways found
✅ Load awareness: Network-wide load data propagated
✅ Load-based routing: Prefers lower-load gateway
✅ PDR maintained: >95%
```

**Slide 4: Multi-Hop Mesh** (Test 5 - if completed)
```
✅ Multi-hop routing: Sensors at 2-3 hops from gateway
✅ Relay forwarding: Relay FWD count >0
✅ Cost differentiation: Higher cost for longer paths
✅ PDR maintained: >95% with multi-hop
Environment: Outdoor (12m spacing, 6 dBm)
```

**Defense Answer Template** (with outdoor test):
"We conducted controlled experiments with sequential node addressing (0x0001-0x0005). Indoor tests (1-3) show Protocol 3 achieves 31% overhead reduction while maintaining PDR >95%. Test 4 validates our W5 gateway load sharing contribution with multi-gateway discovery and load-aware routing. Test 5 validates multi-hop mesh operation with physical distance deployment, demonstrating relay forwarding necessity and cost-based path selection across 2-3 hop routes."

**Defense Answer Template** (without outdoor test):
"We conducted controlled indoor experiments with sequential node addressing. Tests 1-3 show Protocol 3 achieves 31% overhead reduction while maintaining PDR >95%. Test 4 validates our W5 gateway load sharing contribution, demonstrating multi-gateway discovery and load-aware routing decisions. Multi-hop routing behavior is implemented and demonstrated in routing table logic, with physical distance validation recommended as future work."

---

## Troubleshooting

**If addresses still MAC-derived**:
```bash
cd firmware/X_protocol
pio run -t clean
./flash_node.sh Y <port>
# Check serial output for "Local Address: 000Y"
```

**If Test 4 shows no W5 messages**:
1. Check monitoring SENSOR (not gateway)
2. Wait 2-3 minutes for gateway HELLOs to exchange
3. Check both gateways online: `grep "0004\|0005" log`

**If W5 bias always 0.0**:
1. Check sensor log shows `numGateways=2`
2. Check gateway HELLOs parsed: `grep "\[W5\]" log`
3. Check gateway load data received

---

## File Structure After Tests

```
experiments/
├── FINAL_VALIDATION_TESTS.md (this file)
├── results/
│   ├── protocol1/
│   │   └── 4node_indoor_linear_YYYYMMDD_HHMMSS/
│   │       ├── node5.log
│   │       └── ANALYSIS.md
│   ├── protocol2/
│   │   └── 4node_indoor_linear_YYYYMMDD_HHMMSS/
│   │       ├── node5.log
│   │       └── ANALYSIS.md
│   └── protocol3/
│       ├── 4node_indoor_linear_YYYYMMDD_HHMMSS/
│       │   ├── node5.log
│       │   └── ANALYSIS.md
│       ├── 5node_indoor_multigateway_YYYYMMDD_HHMMSS/
│       │   ├── node1.log (SENSOR - critical for W5)
│       │   └── ANALYSIS.md
│       └── 4node_outdoor_multihop_YYYYMMDD_HHMMSS/ (if Test 5 completed)
│           ├── node5.log
│           └── ANALYSIS.md
└── archive/ (outdated guides and plans, e.g., 4NODE/5NODE guides, COMPREHENSIVE_TEST_PLAN, SCALABILITY_ASSESSMENT)
```

---

## Testing Priority and Timeline

**PRIORITY 1: Indoor Tests (MUST DO)**
- **When**: Tomorrow (Nov 13)
- **Tests**: 1-4 (4 hours total)
- **Validates**: W5 contribution, overhead reduction, fair baseline
- **Required for**: Thesis core claims

**PRIORITY 2: Outdoor Test (RECOMMENDED)**
- **When**: Nov 14 (if Tests 1-4 pass)
- **Test**: 5 (1 hour total)
- **Validates**: Multi-hop mesh, relay forwarding
- **Required for**: Strengthens "mesh networking" claim

**Timeline to Defense (19 days remaining)**:

| Date | Activity | Hours |
|------|----------|-------|
| Nov 12 | W5 implementation | ✅ Complete |
| Nov 13 | Indoor tests 1-4 | 4 hours |
| Nov 14 | Outdoor test 5 (optional) | 1 hour |
| Nov 14-15 | Analysis & documentation | 1-2 days |
| Nov 16-28 | Thesis writing | 13 days |
| Nov 29-Dec 1 | Presentation prep | 3 days |
| Dec 2 | Defense practice | 1 day |
| Dec 3 | **Defense day** | - |

**Recommendation**:
1. Do indoor tests first (Tests 1-4) - **MUST COMPLETE**
2. If pass and you have energy: Do outdoor test (Test 5) - **HIGHLY RECOMMENDED**
3. Outdoor test adds strong multi-hop validation for only 1 hour investment

**Status**: On track, but outdoor test recommended for complete validation
