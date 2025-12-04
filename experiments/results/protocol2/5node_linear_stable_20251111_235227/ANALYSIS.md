# Protocol 2 (Hop-Count) - 5-Node Linear Stable Network Test

**Test Date:** November 11, 2025, 23:52-00:22 (next day)
**Test Duration:** ~30 minutes
**TX Power:** 10 dBm
**Topology:** Linear 5-node (S1→S2→R3→R4→G5)
**Test Type:** Stable network (after cold-start convergence)
**Monitoring:** Gateway + Relay + Sensor
**Branch:** xMESH-Test

## Purpose

Test Protocol 2 performance after network has stabilized (routes converged).
Comparison with cold-start test to assess stability.

---

## PDR Analysis - PERFECT ✅

### Packet Delivery

| Metric | Sensor 1 (D218) | Sensor 2 (6674) | Combined |
|--------|----------------|----------------|----------|
| **Expected TX** | 30 | 30 | 60 |
| **Gateway RX** | 30 | 30 | **60** |
| **PDR** | **100%** | **100%** | **100%** |
| **Packet Loss** | 0 | 0 | **0** |

**PDR: 100%** ✅ **PERFECT DELIVERY**

**Sensor1 TX Verified:** 30 packets transmitted (matches gateway RX) ✅

---

## Comparison: Cold-Start vs Stable

### Protocol 2 Performance

| Test Type | PDR | Packet Loss | CRC Errors | Status |
|-----------|-----|-------------|------------|--------|
| **Cold-Start** | 100% | 0/60 | 1 | ✅ Excellent |
| **Stable** | **100%** | 0/60 | **0** | ✅ **PERFECT** |

**Stable network eliminates CRC errors** - even better performance!

---

## 5-Node Protocol 2 Baseline ✅

**Established Metrics:**
- PDR: 100% (both cold-start and stable)
- Packet loss: 0% (stable network)
- CRC errors: 0 (stable network)
- Expected HELLO overhead: ~75 HELLOs (5 nodes × 15 each)

**For Protocol 3 Comparison:**
- **Must match:** 100% PDR
- **Must demonstrate:** ~30% HELLO reduction
- **Target:** 100% PDR + ~50-55 HELLOs (vs 75 baseline)

---

## Test Success

**✅ PROTOCOL 2 5-NODE: VALIDATED**
- Perfect PDR in both cold-start and stable tests
- Scalability to 5 nodes proven
- Baseline established for Protocol 3

---

**Ready for FINAL TEST: Protocol 3 5-node linear!**
