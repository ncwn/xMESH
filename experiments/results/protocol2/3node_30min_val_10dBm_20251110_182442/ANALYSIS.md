# Protocol 2 (Hop-Count Routing) - 30-Minute Validation Test Analysis

**Test Date:** November 10, 2025, 18:24-18:54
**Test Duration:** ~29 minutes
**TX Power:** 10 dBm (reduced from 14 dBm)
**Topology:** Linear 3-node (Sensor→Relay→Gateway)
**Branch:** xMESH-Test

**🚨 STATUS: CRITICAL BUG DETECTED - DIRECT GATEWAY PATH**

---

## Test Configuration

### Node Setup
| Node | ID | Role | Address | Serial Port | Status |
|------|-----|------|---------|-------------|--------|
| Node 1 | 1 | SENSOR (0) | 0x0001 (D218) | /dev/cu.usbserial-0001 | ✅ Correct |
| Node 2 | 3 | RELAY (1) | 0x0003 (6674) | /dev/cu.usbserial-5 | ✅ Correct |
| Node 3 | 5 | GATEWAY (2) | 0x0005 (BB94) | /dev/cu.usbserial-7 | ✅ Correct |

### Radio Configuration
- **Frequency:** 923.2 MHz ✅
- **TX Power:** 10 dBm ✅
- **Spreading Factor:** 7 ✅
- **Bandwidth:** 125.0 kHz ✅
- **Coding Rate:** 4/5 ✅

---

## Packet Delivery Analysis

### PDR Calculation
| Metric | Count | Result |
|--------|-------|--------|
| **Sensor TX** (Node 1) | 29 packets | Seq 0-28 |
| **Gateway RX** (Node 5) | 29 packets | All received |
| **PDR** | **100%** | ✅ **EXCEEDS 95% threshold** |

**Formula:** PDR = (Gateway RX / Sensor TX) × 100% = (29/29) × 100% = **100%**

### Packet Timing
- **Start:** 18:25:49 (00:01:06 after boot)
- **End:** 18:53:57 (00:29:14 after boot)
- **Duration:** ~28 minutes of active transmission
- **Interval:** ~60 seconds average per packet

---

## 🚨 CRITICAL BUG: Direct Gateway Path Detected

### Problem Description

**Expected Behavior:**
Sensor → Relay → Gateway (multi-hop, hops ≥ 1)

**Actual Behavior:**
Sensor → Gateway (direct path, hops = 0)

### Evidence

#### 1. Routing Table Analysis (Node 1 - Sensor)

```
[18:25:13.099] BB94 | BB94 |    1 | 01  ← Gateway at 1 hop (DIRECT)
[18:25:13.103] 6674 | 6674 |    1 | 00  ← Relay also at 1 hop (DIRECT)
```

**Interpretation:**
- Sensor sees gateway at **hops=1** (directly reachable)
- Sensor also sees relay at hops=1
- Both are one-hop neighbors → sensor can reach gateway without relay

#### 2. Gateway Reception Logs (Node 5)

```
[18:25:49.792] GATEWAY: Packet 0 from D218 received (hops=0, value=43.62)
[18:26:46.423] GATEWAY: Packet 1 from D218 received (hops=0, value=0.08)
[18:27:51.098] GATEWAY: Packet 2 from D218 received (hops=0, value=56.56)
...all 29 packets show hops=0
```

**Interpretation:**
- ALL packets arrive with **hops=0** (no relay traversal)
- Gateway receives directly from sensor
- Relay is NOT being used in the routing path

#### 3. Sensor Transmission Logs

```
[18:25:49.395] TX: Seq=0 Value=43.62 to Gateway=BB94 (Hops=1)
```

**Interpretation:**
- Sensor transmits directly to gateway address
- Routing table indicates 1-hop path (direct)

---

## Root Cause Analysis

### Why Did This Happen?

**Hypothesis:** 10 dBm TX power is **too high** for LoRaMesher-based protocols

**Comparison with Protocol 1:**
- **Protocol 1 (Flooding):** Successfully used relay at 10 dBm
- **Protocol 2 (Hop-Count):** Bypassed relay at 10 dBm

**Possible Explanations:**

1. **Different Packet Structures**
   - LoRaMesher uses different packet format than flooding
   - May have better SNR/decoding capability

2. **Power Estimation Mismatch**
   - 10 dBm indoor range may be 15-25m
   - Physical node spacing might be < 15m
   - Gateway within direct range of sensor

3. **HELLO Packet Strength**
   - HELLO packets establish initial routes
   - Direct HELLO reception → direct route added
   - Relay path never preferred (both are 1 hop)

4. **No Relay Preference**
   - Hop-count routing chooses shortest path
   - Direct path (1 hop) = Relay path (1 hop)
   - No reason to prefer relay if both equal

---

## Impact Assessment

### Test Validity

| Aspect | Status | Notes |
|--------|--------|-------|
| **PDR Measurement** | ✅ Valid | 100% delivery confirmed |
| **Multi-Hop Validation** | ❌ INVALID | No multi-hop path used |
| **Hop-Count Routing** | ⚠️ PARTIAL | Works, but chose direct path |
| **Relay Functionality** | ❌ UNTESTED | Relay not in data path |

### Thesis Defense Impact

**🚨 CRITICAL ISSUE FOR THESIS:**
- Cannot claim multi-hop validation at 10 dBm
- Protocol 2 baseline needs re-testing at **8 dBm** or **lower**
- Comparative analysis with Protocol 3 may be invalid

---

## Protocol Behavior Analysis

### ✅ Hop-Count Routing - WORKING (but chose direct path)

**Routing Table Convergence:**
- ✅ Builds routing table from HELLO packets
- ✅ All nodes discover neighbors correctly
- ✅ Routes converge within 30 seconds
- ✅ Stable routing (no flapping)

**Route Selection:**
- ✅ Chooses shortest path by hop count
- ⚠️ Direct path (1 hop) preferred over relay path (also 1 hop)
- ✅ Consistent routing throughout test

### ✅ HELLO Packet Exchange

**HELLO Traffic:**
- Low HELLO count detected (only 3 references in 30 min)
- Expected: ~15 HELLOs per node (120s interval)
- ⚠️ HELLO logs may not be fully captured

### ✅ Packet Delivery

**Reliability:**
- ✅ 100% PDR (29/29 packets)
- ✅ No packet loss
- ✅ Stable communication

---

## Bug Checklist

| Bug | Status | Evidence |
|-----|--------|----------|
| NODE_ID caching | ✅ NOT FOUND | All nodes show correct IDs (1, 3, 5) |
| Direct gateway path | 🚨 **FOUND** | All packets hops=0, relay unused |
| Frequency mismatch | ✅ NOT FOUND | All nodes at 923.2 MHz |
| Library inconsistency | ✅ NOT FOUND | Clean compilation |
| Duty cycle violation | ✅ NOT FOUND | No warnings in logs |
| Route flapping | ✅ NOT FOUND | Stable routing table |
| Communication failure | ✅ NOT FOUND | 100% PDR |

---

## Test Success Criteria

| Criterion | Target | Actual | Status |
|-----------|--------|--------|--------|
| **PDR** | ≥ 95% | 100% | ✅ PASS |
| **Multi-Hop Validation** | Required | ❌ Direct path | ❌ **FAIL** |
| **Routing Convergence** | < 30s | < 30s | ✅ PASS |
| **Duty Cycle** | < 1% | < 1% | ✅ PASS |
| **Duration** | ~30 min | ~29 min | ✅ PASS |

---

## Final Verdict

### ❌ PROTOCOL 2: TEST INVALID - DIRECT GATEWAY PATH

**Summary:**
- ✅ **PDR: 100%** (29/29 packets delivered)
- ✅ **Hop-count routing working correctly**
- ✅ **Routes converge quickly and remain stable**
- ❌ **Multi-hop NOT validated** (direct sensor→gateway path)
- ❌ **Relay unused** (defeats test purpose)
- ⚠️ **10 dBm too high for indoor multi-hop**

**Critical Findings:**
1. All packets received with **hops=0** (direct path)
2. Sensor routing table shows gateway at **1 hop** (direct)
3. Relay present but not in data path
4. Cannot validate multi-hop routing behavior

**Root Cause:**
- 10 dBm power too high for Protocol 2 in current physical setup
- Sensor can directly reach gateway (no relay needed)
- Physical spacing insufficient or obstacles not blocking signal

---

## Required Actions

### 🔴 IMMEDIATE ACTIONS (Before Protocol 3 Test)

1. **Reduce TX Power to 8 dBm**
   - File: `firmware/2_hopcount/src/config.h`
   - Change: `LORA_TX_POWER 10` → `LORA_TX_POWER 8`
   - Reason: Force relay usage

2. **Increase Physical Separation**
   - Option A: Move sensor/gateway further apart (15-20m)
   - Option B: Add physical obstacles (metal cabinets, walls)
   - Goal: Break direct sensor→gateway link

3. **Re-test Protocol 2**
   - Flash with 8 dBm power
   - Run another 30-minute test
   - Verify hops > 0 in gateway logs

### 📋 ALTERNATIVE SOLUTIONS

**Option 1: Accept Direct Path**
- Document that 10 dBm allows direct communication
- Test Protocol 3 at same power for fair comparison
- Note: Loses multi-hop validation

**Option 2: Physical Topology Change**
- Use larger test area (outdoor?)
- Place nodes in separate rooms with walls
- Verify relay becomes necessary

**Option 3: Power Calibration**
- Test multiple power levels: 8, 6, 5 dBm
- Find minimum power that forces relay usage
- Use same power for all protocols

---

## Recommendations

### For Thesis Defense

**Current Status:**
- ❌ **Cannot use this test** for multi-hop validation
- ❌ **Cannot compare** with Protocol 1 fairly (P1 used relay, P2 didn't)
- ⚠️ Must re-test or explain limitation

**Defense Strategy:**
1. **Acknowledge Issue:** "Initial 10 dBm test showed direct paths in Protocol 2"
2. **Show Solution:** "Reduced to 8 dBm to force multi-hop behavior"
3. **Present Valid Data:** Use re-test results with proper multi-hop

### For Protocol 3 Test

**⚠️ CRITICAL DECISION NEEDED:**

Should you:
- **A) Test Protocol 3 at 10 dBm** (for consistency, but may have direct paths)
- **B) Test Protocol 3 at 8 dBm** (for multi-hop validation)
- **C) Re-test Protocol 2 at 8 dBm first, then Protocol 3 at 8 dBm**

**Recommendation:** **Option C** - Re-test Protocol 2 at 8 dBm before proceeding to Protocol 3

---

## Files Generated
- `node1_20251110_182442.log` - Sensor logs (31.6 KB)
- `node2_20251110_182442.log` - Relay logs (26.8 KB)
- `node3_20251110_182442.log` - Gateway logs (34.3 KB)

**Total Test Data:** 92.7 KB

---

## Next Steps

1. **Pause Protocol 3 Testing** ⏸️
2. **Fix TX power to 8 dBm** (Protocol 2 only for now)
3. **Re-flash Protocol 2 nodes**
4. **Run 30-minute validation with 8 dBm**
5. **Verify hops > 0 in logs**
6. **Then proceed to Protocol 3** (at 8 dBm or 10 dBm based on results)
