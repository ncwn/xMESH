# xMESH Comprehensive Test Plan

**Created**: November 10, 2025
**Status**: Indoor suite executed (Nov 10–13) — hallway/outdoor multi-hop still pending
**Hardware Available**: 5 Heltec WiFi LoRa 32 V3 nodes (max)
**Frequency**: 923.2 MHz (AS923 Thailand)

---

## ✅ Status Check (November 13, 2025)

1. **Library alignment** – All protocols now share the same forked LoRaMesher build (gateway load byte included). No regressions observed in Protocols 1 & 2.
2. **Indoor validation** – 14 × 30-minute runs (3–5 nodes) completed Nov 10–12 plus dual-gateway W5 tests Nov 13. Logs reside under `experiments/results/protocol*/`.
3. **Remaining work** – Physical multi-hop/outdoor run (≤8 dBm) + optional long-duration soak. All other requirements satisfied.
4. **Archived context** – The pre-fork tests from Nov 6–9 remain stored in `experiments/results/archive/all_previous_tests_20251110/` for reference but are no longer cited in documentation.

---

## Test Requirements for Validation

### Definition of "Complete Validation"
A protocol is considered VALIDATED only when:
1. ✅ All topology configurations tested (3, 4, 5 nodes)
2. ✅ Three types of tests completed:
   - **Short Tests** (5-10 min): Basic functionality
   - **Physical Tests** (30 min): Real-world deployment (condo area)
   - **Long Tests** (60+ min): Statistical data collection
3. ✅ All test scenarios pass with >95% PDR
4. ✅ Comparative metrics collected across all protocols
5. ✅ Results documented with plots and analysis

**Nov 13 Status:** Items 1–4 are satisfied for the indoor campaign (evidence in `experiments/INDOOR_VALIDATION_SUMMARY.md`). Item 2 still needs the hallway/outdoor multi-hop run plus one long-duration (>60 min) soak to strengthen the thesis dataset, and Item 5 is underway as part of the documentation polish.

---

## Node Configuration

### Available Hardware (5 nodes max)
| NODE_ID | Role | Serial Port (Mac) | Function |
|---------|------|------------------|-----------|
| 1 | SENSOR | /dev/cu.usbserial-0001 | Data generation |
| 2 | SENSOR | /dev/cu.usbserial-0002 | Data generation |
| 3 | RELAY | /dev/cu.usbserial-0003 | Forwarding only |
| 4 | RELAY | /dev/cu.usbserial-0004 | Forwarding only |
| 5 | GATEWAY | /dev/cu.usbserial-0005 | Final destination |

---

## Test Topologies

### Topology 1: Linear 3-Node (Basic)
```
[S1]----10m----[R3]----10m----[G5]
```
- **Purpose**: Basic connectivity and routing
- **Nodes**: Sensor1, Relay3, Gateway5
- **Expected**: Direct routing, 2 hops

### Topology 2: Linear 4-Node (Extended)
```
[S1]----10m----[S2]----10m----[R3]----10m----[G5]
```
- **Purpose**: Test sensor forwarding (Protocol 2 & 3)
- **Nodes**: Sensor1, Sensor2, Relay3, Gateway5
- **Expected**: Multi-hop routing, 3 hops max

### Topology 3: Star 4-Node
```
        [G5]
       / | \
     5m  5m  5m
    /    |    \
  [S1]  [S2]  [R3]
```
- **Purpose**: Test direct gateway connectivity
- **Nodes**: Sensor1, Sensor2, Relay3, Gateway5
- **Expected**: 1-hop routing for all

### Topology 4: Diamond 4-Node (Multi-Path)
```
      [R3]
     /    \
   5m      5m
  /          \
[S1]        [G5]
  \          /
   5m      5m
     \    /
      [R4]
```
- **Purpose**: Test path selection and redundancy
- **Nodes**: Sensor1, Relay3, Relay4, Gateway5
- **Expected**: Path selection based on metric

### Topology 5: Linear 5-Node (Maximum)
```
[S1]----10m----[S2]----10m----[R3]----10m----[R4]----10m----[G5]
```
- **Purpose**: Maximum hop count testing
- **Nodes**: All 5 nodes
- **Expected**: 4 hops maximum

### Topology 6: Mesh 5-Node (Complex)
```
[S1]---10m---[S2]
  |  \     /  |
  |    \ /    |
 10m    X    10m
  |    / \    |
  |  /     \  |
[R3]---10m---[R4]
       |
      10m
       |
      [G5]
```
- **Purpose**: Complex routing decisions
- **Nodes**: All 5 nodes
- **Expected**: Multiple routing paths

---

## Test Types per Topology

### A. Short Tests (5-10 minutes)
**Purpose**: Basic functionality verification
**Location**: Indoor (connected to Mac)
**Data Collection**: Serial monitoring via `multi_node_capture.py`

**Metrics to Verify**:
- Node connectivity
- Route establishment (<30s)
- Packet delivery (>95% PDR)
- Role-based behavior
- Display functionality

### B. Physical Tests (30 minutes)
**Purpose**: Real-world signal propagation
**Location**: Condo area (outdoor/indoor mix)
**Data Collection**: SD card or laptop in field

**Scenarios**:
- Elevator shaft blocking
- Concrete wall penetration
- Distance limits (50-100m urban)
- Moving nodes (topology changes)
- Interference testing

### C. Long Tests (60+ minutes)
**Purpose**: Statistical validation
**Location**: Fixed setup
**Data Collection**: Full logging for analysis

**Metrics to Collect**:
- Packet Delivery Ratio (PDR)
- End-to-end latency
- Network overhead (control packets)
- Route stability
- Power consumption
- Duty cycle compliance

---

## Test Execution Matrix

Each protocol must complete ALL tests marked with ✅:

| Protocol | Topology | Short | Physical | Long | Status |
|----------|----------|--------|----------|------|---------|
| **Protocol 1 (Flooding)** |
| | T1: Linear 3-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T2: Linear 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T3: Star 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T4: Diamond 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T5: Linear 5-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T6: Mesh 5-Node | ⬜ | ⬜ | ⬜ | Pending |
| **Protocol 2 (Hop-Count)** |
| | T1: Linear 3-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T2: Linear 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T3: Star 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T4: Diamond 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T5: Linear 5-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T6: Mesh 5-Node | ⬜ | ⬜ | ⬜ | Pending |
| **Protocol 3 (Gateway-Aware)** |
| | T1: Linear 3-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T2: Linear 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T3: Star 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T4: Diamond 4-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T5: Linear 5-Node | ⬜ | ⬜ | ⬜ | Pending |
| | T6: Mesh 5-Node | ⬜ | ⬜ | ⬜ | Pending |

**Total Tests Required**: 54 tests (3 protocols × 6 topologies × 3 test types)

---

## Research Claims to Validate

### Primary Metrics (Must Validate)
1. **40% traffic reduction** (P3 vs P1)
2. **>95% PDR maintained** (all protocols)
3. **58% HELLO reduction** (P3 vs P2)
4. **<10s topology detection** (P3 Trickle)

### Secondary Metrics (Supporting)
5. Route quality improvement (RSSI/SNR)
6. ETX accuracy without ACKs
7. Gateway bias effectiveness
8. Scalability (3 → 5 nodes)

---

## Test Commands

### Flash Nodes (Example for Protocol 3)
```bash
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 2 /dev/cu.usbserial-0002
./flash_node.sh 3 /dev/cu.usbserial-0003
./flash_node.sh 4 /dev/cu.usbserial-0004
./flash_node.sh 5 /dev/cu.usbserial-0005
```

### Capture Data (Example for 5 nodes)
```bash
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
TOPOLOGY="T6_mesh_5node"
PROTOCOL="protocol3"
TEST_TYPE="short"

mkdir -p experiments/results/$PROTOCOL/${TEST_TYPE}_${TOPOLOGY}_$TIMESTAMP

python3 raspberry_pi/multi_node_capture.py \
  --nodes 1,2,3,4,5 \
  --ports /dev/cu.usbserial-0001,/dev/cu.usbserial-0002,/dev/cu.usbserial-0003,/dev/cu.usbserial-0004,/dev/cu.usbserial-0005 \
  --baudrate 115200 \
  --duration 600 \
  --output experiments/results/$PROTOCOL/${TEST_TYPE}_${TOPOLOGY}_$TIMESTAMP
```

### Analyze Results
```bash
python3 raspberry_pi/data_analyzer.py \
  experiments/results/$PROTOCOL/${TEST_TYPE}_${TOPOLOGY}_$TIMESTAMP/*.log \
  --plot --report --output analysis_${TOPOLOGY}.json
```

---

## Success Criteria

### Per-Protocol Success
- ✅ All 6 topologies tested
- ✅ All 3 test types completed
- ✅ >95% PDR in all scenarios
- ✅ <1% duty cycle maintained
- ✅ Results documented with plots

### Comparative Success
- ✅ Clear traffic reduction demonstrated (P3 vs P1)
- ✅ HELLO overhead reduction proven (P3 vs P2)
- ✅ Scalability verified (3 → 5 nodes)
- ✅ Statistical significance achieved (p < 0.05)

---

## Timeline

### Week 1 (Nov 11-17): Protocol Testing
- Day 1-2: Protocol 1 all tests
- Day 3-4: Protocol 2 all tests
- Day 5-6: Protocol 3 all tests
- Day 7: Data compilation

### Week 2 (Nov 18-24): Analysis
- Comparative analysis
- Plot generation
- Statistical testing
- Report writing

### Week 3 (Nov 25-Dec 2): Finalization
- Final report
- Presentation preparation
- Defense practice
- Submission (Dec 3)

---

## Notes

1. **IMPORTANT**: Do NOT proceed to next protocol until current protocol passes ALL tests
2. **Frequency**: Ensure 923.2 MHz for ALL tests (AS923 Thailand)
3. **Library**: All protocols must use `https://github.com/ncwn/xMESH.git#main`
4. **Documentation**: Create SUMMARY.md for each test immediately after completion
5. **Backup**: Save raw logs to external drive after each test

---

**Status**: AWAITING EXECUTION
**Next Step**: Begin with Protocol 1, Topology 1, Short Test
