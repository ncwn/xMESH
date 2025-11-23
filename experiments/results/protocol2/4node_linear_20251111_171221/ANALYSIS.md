# Protocol 2 (Hop-Count) - 4-Node Linear Topology Test Analysis

**Test Date:** November 11, 2025, 17:12-17:41
**Test Duration:** ~29 minutes
**TX Power:** 10 dBm
**Topology:** Linear 4-node (S1→S2→R3→G5)
**Monitoring:** Relay (Node 3) + Gateway (Node 5)
**Branch:** xMESH-Test

⚠️ **PDR: 81.7% (BELOW 95% - Consistent with Protocol 1 Baseline)**

---

## Test Configuration

### Node Setup
| Node | ID | Role | Address | Monitoring |
|------|-----|------|---------|------------|
| Node 1 | 1 | SENSOR | D218 | ❌ Battery |
| Node 2 | 2 | SENSOR | 6674 | ❌ Battery |
| Node 3 | 3 | RELAY | 02B4 | ✅ USB |
| Node 5 | 5 | GATEWAY | BB94 | ✅ USB |

**Note:** Addresses are LoRaMesher-generated (not simple NODE_ID mapping)

---

## PDR Analysis - Below Threshold (Baseline Established)

### Packet Delivery

| Sensor | Address | Received | Expected | PDR | Packet Loss |
|--------|---------|----------|----------|-----|-------------|
| **Sensor 1** | D218 | 25 | 30 | **83.3%** | 5 packets |
| **Sensor 2** | 6674 | 24 | 30 | **80.0%** | 6 packets |
| **Combined** | - | **49** | **60** | **81.7%** | **11 packets** |

**PDR: 81.7%** (below 95% threshold)

---

## Comparison with Protocol 1 (Flooding)

| Metric | Protocol 1 | Protocol 2 | Change |
|--------|-----------|-----------|--------|
| **PDR** | 85.0% | **81.7%** | -3.3% (worse) |
| **Sensor 1 PDR** | 83.3% | 83.3% | Same |
| **Sensor 2 PDR** | 86.7% | **80.0%** | -6.7% (worse) |
| **Packet Loss** | 9/60 | **11/60** | +2 packets lost |
| **CRC Errors** | 7 | 4 | -3 (better) |

**Key Finding:** Protocol 2 PDR slightly **worse** than Protocol 1 despite fewer CRC errors

**Possible Explanation:**
- Hop-count routing uses unicast (more precise delivery requirements)
- Flooding broadcasts (redundant paths may help despite collisions)
- Both protocols struggle under dense 4-node, 10 dBm conditions

---

## Routing Behavior Analysis

### Gateway Routing Table

**Final State (3 entries):**
```
D218 | D218 | 1 | 00  ← Sensor 1 (direct, 1 hop)
6674 | 6674 | 1 | 00  ← Sensor 2 (direct, 1 hop)
02B4 | 02B4 | 1 | 00  ← Relay (direct, 1 hop)
```

**All nodes at 1 hop** = Gateway sees ALL neighbors directly

**Convergence Time:** ~30 seconds (routing table complete by 00:02:30)

---

## Direct Path Issue (Same as 3-Node Test)

**Evidence:**
- All packets show `hops=0` at gateway
- Routing table: All nodes at 1 hop (direct neighbors)
- **No multi-hop routing** despite 4-node linear topology

**Root Cause:**
- 4m radius with 10 dBm = all nodes within direct range
- Gateway can hear all sensors and relay
- Routing not actually using multi-hop paths

**Impact on Thesis:**
- ⚠️ Cannot claim multi-hop validation at 10 dBm
- ✅ CAN claim hop-count routing works
- ✅ CAN use as overhead baseline for Protocol 3 comparison

---

## HELLO Overhead - VERY LOW (Suspicious)

**Count:** Only 1 HELLO reference in logs

**Expected for 30 minutes:**
- 4 nodes × 15 HELLOs (120s interval) = **60 HELLOs**

**Actual:** ~1-2 HELLOs captured

**Issue:** Gateway logs may not be capturing all HELLO packets, OR HELLOs are not being logged to serial output properly

**Cannot establish baseline** without HELLO count

---

## CRC Errors

**Count:** 4 total (1 relay + 3 gateway)

**Comparison with Protocol 1:**
- Protocol 1: 7 CRC errors
- Protocol 2: 4 CRC errors (-43% fewer)

**Better than flooding** but still indicates RF quality issues

---

## Test Success Criteria

| Criterion | Target | Actual | Status |
|-----------|--------|--------|--------|
| **PDR** | ≥ 95% | **81.7%** | ❌ FAIL |
| **Multi-Hop** | Validated | Direct paths only | ❌ NOT VALIDATED |
| **Routing Convergence** | <60s | ~30s | ✅ PASS |
| **Errors** | Minimal | 4 CRC errors | ⚠️ CONCERN |

---

## Key Findings for Baseline

### ✅ Useful Baseline Data

1. **PDR Baseline**: 81.7% (similar to Protocol 1's 85%)
2. **Routing Works**: Hop-count table building functional
3. **Fast Convergence**: Routes stable within 30s
4. **Direct Paths**: 4m radius @ 10 dBm = all direct

### ⚠️ Cannot Baseline

1. **HELLO Overhead**: Logs incomplete (only 1 HELLO captured)
2. **Multi-Hop**: Not validated (all direct paths)

---

## Comparison Summary: Protocol 1 vs Protocol 2

| Metric | P1 (Flooding) | P2 (Hop-Count) | Winner |
|--------|--------------|---------------|--------|
| **PDR** | 85.0% | 81.7% | P1 (+3.3%) |
| **Sensor 1 PDR** | 83.3% | 83.3% | TIE |
| **Sensor 2 PDR** | 86.7% | 80.0% | P1 (+6.7%) |
| **CRC Errors** | 7 | 4 | P2 (-43%) |
| **Packet Loss** | 9 | 11 | P1 (-2 packets) |

**Unexpected:** Flooding slightly outperforms hop-count in this dense scenario!

**Hypothesis:** Flooding's broadcast redundancy helps in high-interference environment

---

## Critical Issue: Missing HELLO Data

**Problem:** Cannot validate HELLO overhead baseline (only 1 HELLO in logs)

**Possible Causes:**
1. HELLO packets not being logged to serial
2. Logging disabled in Protocol 2
3. Gateway filtering HELLO log output

**Impact:** Cannot measure Protocol 3's overhead reduction without Protocol 2 baseline

**Recommendation:** Check Protocol 2 logging configuration before final thesis claims

---

## Verdict for Baseline Comparison

**PDR Baseline Established:** ✅
- Protocol 1: 85.0%
- Protocol 2: 81.7%
- **Both struggle at 4-node, 10 dBm, 4m radius**

**HELLO Baseline:** ❌ NOT ESTABLISHED
- Need HELLO count to validate Protocol 3's overhead reduction

---

## Next Steps

### Proceed to Protocol 3 Test

**Critical Questions:**
1. **Does Protocol 3 PDR ≥ 82%?** (match baseline)
2. **Does Protocol 3 PDR > 85%?** (exceed baseline) ← IDEAL
3. **Can we count HELLOs?** (establish overhead reduction)

**If Protocol 3 achieves 90%+ PDR:**
- **Excellent thesis narrative:** "Protocol 3 excels where baselines struggle"
- Demonstrates robustness through intelligent routing
- Overhead reduction is bonus on top of better PDR

**If Protocol 3 matches 80-85% PDR:**
- **Acceptable thesis:** "Maintains baseline PDR while reducing overhead"
- Focus on efficiency gains, not reliability improvement
- Acknowledge challenging RF environment

---

**Test Protocol 3 now to see the comparison!**
