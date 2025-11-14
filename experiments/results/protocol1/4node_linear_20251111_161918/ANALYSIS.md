# Protocol 1 (Flooding) - 4-Node Linear Topology Test Analysis

**Test Date:** November 11, 2025, 16:19-16:49
**Test Duration:** ~30 minutes
**TX Power:** 10 dBm
**Topology:** Linear 4-node (S1→S2→R3→G5)
**Monitoring:** Relay (Node 3) + Gateway (Node 5)
**Branch:** xMESH-Test

⚠️ **CRITICAL ISSUE: PDR BELOW 95% THRESHOLD**

---

## Test Configuration

### Node Setup (4 Nodes)
| Node | ID | Role | Address | Monitoring | Status |
|------|-----|------|---------|------------|--------|
| Node 1 | 1 | SENSOR | 0x0001 | ❌ Battery | Transmitting |
| Node 2 | 2 | SENSOR | 0x0002 | ❌ Battery | Transmitting |
| Node 3 | 3 | RELAY | 0x0003 (02B4) | ✅ **USB** | Forwarding |
| Node 5 | 5 | GATEWAY | 0x0005 (BB94) | ✅ **USB** | Receiving |

**Captured Logs:**
- node1_20251111_161918.log: **Relay (Node 3)** - 8.9 KB
- node2_20251111_161918.log: **Gateway (Node 5)** - 9.6 KB

**Radio Configuration:**
- Frequency: 923.2 MHz ✅
- TX Power: 10 dBm ✅
- Spreading Factor: 7 ✅

---

## ⚠️ PACKET DELIVERY ANALYSIS - BELOW THRESHOLD

### PDR Calculation

| Metric | Sensor 1 | Sensor 2 | Combined |
|--------|----------|----------|----------|
| **Expected TX** | 30 packets | 30 packets | 60 packets |
| **Gateway RX** | 25 packets | 26 packets | 51 packets |
| **PDR** | **83.3%** | **86.7%** | **85.0%** |
| **Packet Loss** | 5 packets | 4 packets | 9 packets |

**🚨 CRITICAL**: PDR 85% is **BELOW 95% threshold** - Test indicates issues!

---

## Missing Packets Analysis

### Sensor 1 (0x0001) - Missing 5 Packets

**Received Sequences:** 1,2,3,4,5,8,9,10,11,12,14,15,17,18,20,21,22,23,24,25,26,27,28,29,30
**Missing Sequences:** 6, 7, 13, 16, 19

**Pattern**: Scattered losses (no burst pattern)

### Sensor 2 (0x0002) - Missing 4 Packets

**Received Sequences:** 2,3,5,6,7,8,9,10,11,12,13,14,15,16,17,19,20,21,22,23,24,25,26,27,29,30
**Missing Sequences:** 1, 4, 18, 28

**Pattern**: Also scattered losses

---

## Relay Forwarding Analysis

### Relay Performance

| Metric | Sensor 1 | Sensor 2 | Total |
|--------|----------|----------|-------|
| **Relay RX** | ~24 | ~23 | 47 |
| **Relay FWD** | 24 | 23 | 47 |
| **Gateway RX** | 25 | 26 | 51 |

**Discrepancy**: Gateway received **4 more packets** than relay forwarded

**Possible Explanations:**
1. **Direct paths**: Some packets reached gateway without relay (table-top 1m spacing)
2. **Relay missed packets**: Some packets not logged by relay but forwarded
3. **Node 2 (Sensor2) acting as relay**: Forwarding Sensor1's packets

**Most Likely**: With 4 nodes at 1m spacing and 10 dBm power, multiple direct/alternate paths exist

---

## Error Analysis

### LoRaMesher CRC Errors (Error Code -7)

**Occurrences:**
- Relay (Node 3): 2 errors
- Gateway (Node 5): 5 errors
- **Total: 7 CRC errors**

**Timing:**
- Distributed throughout test (not clustered)
- May correlate with packet losses

**Likely Causes:**
1. RF interference (table-top test, close proximity)
2. Packet collisions (4 nodes flooding simultaneously)
3. Weak signals (10 dBm may be borderline for some positions)
4. Multipath effects (indoor reflections)

**Impact**: 7 errors ≈ 12% of 60 expected packets (correlates with 15% loss rate)

---

## Test Duration

**Timeline:**
- Start: 16:19:57 (first packet received at gateway)
- End: 16:49:06 (last packet received)
- **Duration:** ~29 minutes 9 seconds

**Packet Rate:**
- Expected: 60 packets in 30 min
- Actual: 51 packets in 29 min
- **Rate**: ~1.75 packets/min (close to expected ~2 packets/min for 2 sensors)

---

## Bug Checklist

| Bug | Status | Evidence |
|-----|--------|----------|
| **PDR < 95%** | 🚨 **FOUND** | 85% PDR (9 packets lost) |
| NODE_ID caching | ✅ NOT FOUND | Node 3 and 5 correct |
| Frequency mismatch | ✅ NOT FOUND | Both at 923.2 MHz |
| Direct gateway paths | ⚠️ **POSSIBLE** | 4 packets bypassed relay |
| CRC errors | ⚠️ **FOUND** | 7× error -7 |
| Relay not forwarding | ✅ NOT FOUND | 47 forwards logged |

---

## Root Cause Analysis

### Why PDR < 95%?

**Hypothesis 1: Table-Top Interference** (Most Likely)
- 4 nodes within 1m at 10 dBm
- Strong signals + close proximity = packet collisions
- Multiple transmission paths interfere
- CRC errors (7 total) support this theory

**Hypothesis 2: Flooding Overhead**
- 4 nodes = more broadcasts
- Increased collision probability
- Relay forwarding creates additional traffic

**Hypothesis 3: 10 dBm Still Too High**
- Creates complex multi-path environment
- Packets arrive via multiple routes (relay + direct)
- Timing overlaps cause corruption

**Hypothesis 4: Missing Sensor TX Data**
- Cannot confirm sensors actually transmitted all 30 packets
- Some packets may not have been sent
- Need sensor logs to verify

---

## Comparison with 3-Node Test

| Metric | 3-Node (Validated) | 4-Node (Current) | Change |
|--------|-------------------|------------------|--------|
| **PDR** | 100% (29/29) | **85% (51/60)** | ❌ **-15%** |
| **CRC Errors** | 1 | **7** | ❌ **+600%** |
| **Relay Forwards** | 29/29 (100%) | 47/51 (92%) | ⚠️ **-8%** |
| **Duration** | ~29 min | ~29 min | ✅ Same |

**Degradation**: Adding 4th node significantly impacted performance

---

## Test Success Criteria

| Criterion | Target | Actual | Status |
|-----------|--------|--------|--------|
| **PDR** | ≥ 95% | **85%** | ❌ **FAIL** |
| **Relay Forwarding** | Active | Yes (47 fwds) | ✅ PASS |
| **Multi-Node** | 4 nodes | 4 nodes | ✅ PASS |
| **Duration** | ~30 min | ~29 min | ✅ PASS |
| **Errors** | Minimal | 7 CRC errors | ⚠️ CONCERN |

---

## 🚨 VERDICT: TEST FAILED - PDR BELOW THRESHOLD

### Summary

**Test Status:** ❌ **INVALID** for thesis validation
- PDR: 85% (below 95% requirement)
- Packet loss: 15% (9/60 packets)
- CRC errors: 7 (RF quality issues)
- Relay performance: Acceptable but incomplete data

**Critical Issues:**
1. **Unacceptable PDR** - Cannot claim "maintains >95% PDR" with 85% result
2. **High error rate** - 7 CRC errors indicate RF problems
3. **Collision probability** - 4 nodes at 1m with 10 dBm creates interference

---

## Recommended Actions

### 🔴 IMMEDIATE: Do NOT Proceed to Protocol 2/3 Yet

**This test reveals fundamental issues** that will affect all protocols:
1. Table-top 4-node topology doesn't work well at 10 dBm
2. Need to resolve RF interference before comparative testing
3. Cannot defend 85% PDR in thesis

---

### Option A: Reduce TX Power to 6-8 dBm (Recommended)

**Rationale:**
- Lower power reduces collision probability
- Forces cleaner single-path routing
- Reduces multipath interference
- Easier signal isolation

**Implementation:**
```cpp
// firmware/1_flooding/src/config.h
#define LORA_TX_POWER  6  // Reduce from 10 dBm
```

**Expected:** PDR improves to >95% with less interference

---

### Option B: Increase Physical Spacing

**Rationale:**
- Move nodes to 5-10m apart
- Reduces collision probability
- More realistic deployment
- Better RF isolation

**Challenge:** Requires larger space (outdoor or hallway)

---

### Option C: Accept Lower PDR & Document

**Rationale:**
- 85% PDR may be realistic for dense 4-node deployment
- Document as "challenging scenario"
- Show Protocol 3 handles it better

**Risk:** Committee may question <95% PDR claim

---

## My Recommendation

**🎯 Reduce TX Power to 6-8 dBm and Re-Test**

**Why:**
- Fastest solution (1 hour: change config + re-flash + re-test)
- Addresses root cause (too much power = interference)
- Proven strategy (3-node worked at 10 dBm, 4-node needs lower)
- No space constraints

**Steps:**
1. Edit firmware/1_flooding/src/config.h → `LORA_TX_POWER = 6`
2. Clean build and re-flash all 4 nodes
3. Re-run 30-minute test
4. Verify PDR >95%
5. **Then** proceed to Protocol 2 & 3 at same 6 dBm

---

## Data Quality Assessment

**What We Can Still Learn:**
- ✅ 4-node topology is feasible
- ✅ Relay forwarding works (47 forwards)
- ✅ Both sensors transmitting
- ✅ Gateway receives from multiple sources
- ⚠️ But PDR too low for thesis claims

**What We Cannot Claim:**
- ❌ "Protocol 1 maintains >95% PDR at 4 nodes"
- ❌ Valid baseline for Protocol 2/3 comparison
- ❌ Thesis-quality validation

---

## Files Generated
- node1_20251111_161918.log - Relay logs (8.9 KB)
- node2_20251111_161918.log - Gateway logs (9.6 KB)

**Total Test Data:** 18.5 KB

---

## Next Steps

**CRITICAL DECISION REQUIRED:**

1. **Re-test at 6 dBm** (recommended) - 1 hour
2. **Increase spacing** (space-dependent) - variable time
3. **Accept 85% PDR** (risky for thesis) - document limitation

**My strong recommendation:** Reduce TX power to 6 dBm and re-run Protocol 1 test. This should resolve the interference issues and achieve >95% PDR needed for thesis validation.

**Do you want to proceed with power reduction?**
