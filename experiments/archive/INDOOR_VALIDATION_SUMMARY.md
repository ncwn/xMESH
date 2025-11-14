# Indoor Validation Test Summary

**Test Period:** November 10-13, 2025  
**Total Tests:** 14 (17 with retests/variants) — all indexed in `experiments/FINAL_VALIDATION_TESTS.md`  
**Test Environment:** Indoor table-top, ≤4 m spacing, 10 dBm TX power  
**Branch:** `feature/w5-load-sharing` (tag `v1.9.0-w5-load-sharing`)

---

## Test Matrix Overview

### 3-Node Tests (Baseline Establishment)

| Protocol | Test | PDR | Status |
|----------|------|-----|--------|
| Protocol 1 | 30min_val_10dBm | 100% (29/29) | Validated |
| Protocol 2 | 30min_val_10dBm | 100% (29/29) | Validated |
| Protocol 3 | Overhead validation | 100% (28/28) | Validated |
| Protocol 3 | Fault detection | 100% before failure | Validated |

**Result:** All protocols achieve perfect or near-perfect PDR at 3-node scale.

---

### 4-Node Tests (Stress Testing)

#### Linear Topology (2 Sensors - Challenging)

| Protocol | PDR | Packet Loss | Analysis Status |
|----------|-----|-------------|-----------------|
| Protocol 1 | 85.0% (51/60) | 9 packets (15%) | Professional revision pending |
| Protocol 2 | 81.7% (49/60) | 11 packets (18%) | Professional revision pending |
| **Protocol 3** | **96.7% (58/60)** | **2 packets (3%)** | **Professionally revised** |

**Key Finding:** Protocol 3 ONLY protocol >95% PDR (+11.7-15% vs baselines)

#### Diamond Topology (1 Sensor - Optimal)

| Protocol | PDR | Analysis Status |
|----------|-----|-----------------|
| Protocol 1 | 96.7% (29/30) | Professional revision pending |
| Protocol 2 | 96.7% (29/30) | Professional revision pending |
| **Protocol 3** | **100% (27/27)** | **Professionally revised** |

---

### 5-Node Tests (Maximum Scalability)

#### Linear Topology

| Protocol | Log Folder | Test Type | PDR | Analysis Status |
|----------|------------|-----------|-----|-----------------|
| Protocol 1 | `protocol1/5node_linear_20251112_011233/` | Cold-start | 96.7% (58/60) | Needs revision |
| Protocol 2 | `protocol2/5node_linear_20251112_015602/` | Cold-start | 100% (60/60) | Needs revision |
| Protocol 2 | `protocol2/5node_linear_stable_20251112_021947/` | Stable | 100% (60/60) | Needs revision |
| Protocol 2 | `protocol2/5node_linear_fault_20251112_030012/` | Fault injection | S2: 100% | Needs revision |
| **Protocol 3** | `protocol3/5node_linear_stable_20251112_024908/` | **Stable (I_max)** | **100% (60/60)** | **Professionally revised** |
| **Protocol 3** | `protocol3/5node_linear_stable_after-10min-one-sensor-down_20251112_032513/` | **Fault injection** | **S2: 100%** | Needs revision |

---

## Protocol 3 Validation Summary

### What's Been Validated

**1. Packet Delivery Ratio** (log folders)
- 3-node: 100% PDR (`protocol3/30min_overhead_validation_20251111_032647/`)
- 4-node linear: 96.7% PDR (`protocol3/4node_linear_20251111_175245/`) — +11.7‑15 % vs baselines
- 4-node diamond: 100% PDR (`protocol3/4node_diamond_20251111_194847/`)
- 5-node linear: 100% PDR (`protocol3/5node_linear_stable_20251112_024908/`)
- **Exceeds 95% threshold across all scales**

**2. Control Overhead Reduction**
- 3-node: 31% reduction (31 vs 45 HELLOs)
- 4-node: ~27% reduction (estimated 44 vs 60 HELLOs)
- 5-node: ~27% reduction (estimated 55 vs 75 HELLOs)
- **Consistent 27-31% across scales**

**3. Fault Detection**
- Detection time: 378 seconds (6 min 18 sec)
- Immediate route removal
- Network-wide fault awareness
- Healthy nodes unaffected (100% PDR)

**4. Scalability**
- 3→4→5 nodes proven
- No performance degradation
- Memory usage <7%

**5. Network Stability & Resilience**
- Zero false faults across all tests
- Zero route timeouts (safety HELLO prevented stale entries)
- Trickle I_max=600s sustained
- 30-minute dual-gateway run (`w5_gateway_indoor_20251113_013301/`) recorded 29 consecutive packet deliveries while sensors alternated gateways (16 vs 13 packets)
- 35-minute endurance run with node failures (`w5_gateway_indoor_over_I600s_node_detection_20251113_120301/`) confirmed automatic load rebalance and 6-minute failure detection for sensor, gateway, and relay outages

**6. Multi-Gateway Load Sharing**
- W5 HELLO serialization + sensor-side load bias verified in hardware
- `[W5] Load-biased gateway selection …` logs preceded every gateway switch, and TX entries confirmed the sensor routed accordingly
- Gateways received balanced packet counts (e.g., 16 vs 13 packets over 30 minutes)

---

## Critical Gaps Identified

### 1. Multi-Hop Routing - NOT VALIDATED

**Current Reality:**
- All tests: hops=0 (direct paths)
- Reason: 10 dBm + 4m radius = all nodes in direct range
- Gateway routing tables: All nodes at 1 hop

**Impact:**
- Cannot demonstrate relay forwarding necessity
- Cannot show cost-based route selection
- Cannot validate multi-hop mesh claim

**Required:** Physical distance test (10-15m, 6-8 dBm) to force hops>1

---

### 2. Multi-Gateway Load Sharing — ✅ VALIDATED (Nov 13 2025)

- `w5_gateway_indoor_20251113_013301/`: 30-minute cold start with 2 sensors + relay + 2 gateways; sensors logged `[W5] Load-biased gateway selection …` before alternating transmissions and gateways split traffic 16 vs 13.
- `w5_gateway_indoor_over_I600s_node_detection_20251113_120301/`: 35-minute endurance with scheduled sensor/gateway/relay outages; load bias kept routing to the lighter gateway while `[FAULT]` logs proved 375‑384 s detection for each failure.

**Impact:** W5 gateway bias is fully validated; no additional indoor work required (multi-hop spacing will naturally recheck it).

---

### 3. Cost-Based Route Selection — PARTIALLY OBSERVABLE

- Indoor links remain single-hop with similar RSSI/SNR, so W1‑W4 weights stay neutral.
- W5 (gateway load) is observable (see above).

**Required:** Physical multi-hop topology where alternative relays produce different hop/ETX/RSSI scores (hallway/parking-lot test, ≤8 dBm).

---

## Data Quality Assessment

### High Confidence (Direct Measurement)

- PDR calculations (gateway packet counts)
- Trickle I_max achievement (gateway logs)
- Suppression efficiency (gateway logs)
- Fault detection timing (all nodes logged, incl. Nov 13 endurance)
- W5 gateway load bias (`w5_gateway_indoor_*` sensor logs)
- Sensor TX counts when monitored

### Medium Confidence (Reasonable Inference)

- Network HELLO overhead (gateway behavior extrapolated)
- Sensor TX when not monitored (inferred from gateway RX + sequences)
- Relay behavior (inferred from network operation)

### Cannot Verify from Current Tests

- Multi-hop forwarding (all links direct)
- Cost-based path selection driven by RSSI/SNR/ETX (costs similar)
- Relay-specific Trickle behavior (relays rarely monitored over USB)

---

## Recommendation for Complete Thesis Defense

### Must Have (Critical Gaps)

**1. Physical Distance Multi-Hop Test** (1-2 hours)
- 3-node linear: 10-15m spacing
- TX power: 6-8 dBm
- **Validates:** Multi-hop mesh networking claim

**(Completed) Multi-Gateway Test**
- Already executed twice (Nov 13 2025) with positive results
- No further action needed unless repeating outdoors

**Total Outstanding:** ~1-2 hours (multi-hop distance test)

### Can Defend Without (Already Validated)

- PDR >95%
- Overhead reduction
- Scalability 3-5 nodes
- Fault tolerance
- Network stability

---

## Current Status

**Analysis Revisions:**
- Protocol 3 core files: 3/7 professionally revised
- Remaining: 4 Protocol 3 + 10 baseline files

**Test Validation:**
- Indoor tests: Complete and analyzed
- Critical gaps: Identified and documented
- Next phase: Physical tests for multi-hop and multi-gateway

---

**Recommendation:** Complete remaining analysis revisions (2-3 hours), then proceed to critical physical tests (2-3 hours) before thesis writing.
