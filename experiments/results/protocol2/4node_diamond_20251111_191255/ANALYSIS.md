# Protocol 2 (Hop-Count) - 4-Node Diamond Topology Test Analysis

**Test Date:** November 11, 2025, 19:12-19:42
**Test Duration:** ~30 minutes
**TX Power:** 10 dBm
**Topology:** Diamond 4-node (S1 with dual relay paths to G5)
**Monitoring:** Relay4 (Node 4) + Gateway (Node 5)
**Branch:** xMESH-Test

✅ **PDR: 96.7% - MATCHES Protocol 1 Baseline**

---

## Test Configuration

### Node Setup
| Node | ID | Role | Address | Monitoring |
|------|-----|------|---------|------------|
| Node 1 | 1 | SENSOR | D218 | ❌ Battery |
| Node 3 | 3 | RELAY | 02B4 | ❌ Battery |
| Node 4 | 4 | RELAY | 6674 | ✅ USB |
| Node 5 | 5 | GATEWAY | BB94 | ✅ USB |

**Diamond Layout:** Sensor has TWO potential relay paths to gateway

---

## PDR Analysis - EXCELLENT ✅

### Packet Delivery

| Metric | Count | Result |
|--------|-------|--------|
| **Expected TX** | 30 packets | Sensor1 × 30 |
| **Gateway RX** | 29 packets | Seq 0-29 (missing 1) |
| **PDR** | **96.7%** | ✅ **EXCEEDS 95%** |
| **Packet Loss** | 1 packet | Minimal |

**PDR: 96.7%** - Matches Protocol 1 diamond exactly

---

## Route Selection Analysis - DIRECT PATH

### Gateway Routing Table

**All nodes at 1 hop (direct):**
```
D218 | D218 | 1 | 00  ← Sensor (direct path)
02B4 | 02B4 | 1 | 00  ← Relay3 (direct)
6674 | 6674 | 1 | 00  ← Relay4 (direct)
```

**All packets received with hops=0** (direct from sensor)

**Key Finding:** **No multi-hop routing** - Sensor reaches gateway directly

**Reason:** 4m radius + 10 dBm = All nodes within direct range

**Impact:**
- Cannot test relay path selection (both relays available but not used)
- Direct communication bypasses routing logic
- **Same issue as all previous 10 dBm tests**

---

## Comparison: Protocol 1 vs Protocol 2 (Diamond)

| Metric | Protocol 1 | Protocol 2 | Change |
|--------|-----------|-----------|--------|
| **PDR** | 96.7% | 96.7% | **IDENTICAL** |
| **Packet Loss** | 1/30 | 1/30 | Same |
| **Path Used** | Broadcast | Direct | Both avoid relays |
| **Hops** | N/A (flooding) | 0 (direct) | N/A |

**Verdict:** Both protocols perform identically in diamond topology with direct paths

---

## Multi-Topology Comparison

### Protocol 2 Performance by Topology

| Topology | Sensors | PDR | Path Used | Status |
|----------|---------|-----|-----------|--------|
| **Linear** | 2 | 81.7% | Direct (hops=0) | Below threshold |
| **Diamond** | 1 | **96.7%** | Direct (hops=0) | ✅ Exceeds threshold |

**Key Pattern:** Single sensor = high PDR, dual sensors = lower PDR (collision effect)

---

## Test Success Criteria

| Criterion | Target | Actual | Status |
|-----------|--------|--------|--------|
| **PDR** | ≥ 95% | **96.7%** | ✅ **PASS** |
| **Route Selection** | Multi-hop | Direct only | ❌ NOT VALIDATED |
| **Errors** | Minimal | Low | ✅ PASS |
| **Baseline** | Established | Yes | ✅ PASS |

---

## Verdict

**✅ PROTOCOL 2 DIAMOND: PDR VALIDATED** (96.7%)
**⚠️ Route Selection: NOT VALIDATED** (all direct paths)

**Baseline Established:**
- Protocol 1: 96.7% PDR (flooding)
- Protocol 2: 96.7% PDR (hop-count)
- **Both identical** in diamond single-sensor scenario

**For Protocol 3:**
- Expected PDR: ~96.7% (match baseline)
- **Differentiator:** HELLO overhead (should be ~40% less)
- Route selection: Still direct paths (cannot demonstrate with current power/spacing)

---

## Key Insight

**Direct Path Issue Persists:**
- 10 dBm + 4m radius = All nodes in direct range
- **Cannot force multi-hop** at this configuration
- All topologies show hops=0 at gateway

**Solutions for Future:**
- Reduce TX power to 4-6 dBm (force relay usage)
- Increase spacing to 10-15m (physical distance)
- **OR:** Accept direct paths, focus on PDR and overhead comparison

**For Current Tests:**
- ✅ PDR comparison valid (all same conditions)
- ✅ Overhead comparison valid (HELLO reduction)
- ⚠️ Route selection not demonstrable (all direct)

---

## Files Generated
- node1_20251111_191255.log - Relay4 logs (30.1 KB)
- node2_20251111_191255.log - Gateway logs (37.5 KB)

**Total Test Data:** 67.6 KB

---

## Ready for Protocol 3 Diamond Test

**Expected:**
- PDR: ~96.7% (match baselines)
- HELLO overhead: Reduced vs Protocol 2
- Path: Still likely direct (10 dBm limitation)
- **Demonstration:** Efficiency gains with equivalent PDR

**Flash Protocol 3 for final diamond comparison!**
