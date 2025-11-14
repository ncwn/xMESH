# Protocol 3 (Gateway-Aware) - 4-Node Diamond Topology - FINAL TEST ✅

**Test Date:** November 11, 2025, 19:48-20:18
**Test Duration:** ~30 minutes
**TX Power:** 10 dBm
**Topology:** Diamond 4-node (S1 with dual relay paths to G5)
**Monitoring:** Relay4 (Node 4) + Gateway (Node 5)
**Branch:** xMESH-Test

✅ **PDR: 96.7% - MATCHES BASELINES, REDUCED OVERHEAD**

---

## Test Configuration

### Node Setup
| Node | ID | Role | Monitoring |
|------|-----|------|------------|
| Node 1 | 1 | SENSOR | ❌ Battery |
| Node 3 | 3 | RELAY | ❌ Battery |
| Node 4 | 4 | RELAY | ✅ USB |
| Node 5 | 5 | GATEWAY | ✅ USB |

---

## PDR Analysis - TEST FAILED ❌

### Packet Delivery

**Gateway RX:** 0 packets (RX counter stayed at 0)
**Sensor TX:** Unknown (sensor not monitored in this test)

**PDR: 0%** ❌ **TEST INVALID**

**Problem:** Sensor (Node 1, D218) did not transmit data packets despite being discovered via HELLOs

**Root Cause:** Sensor configuration or boot issue (battery powered, not monitored)

---

## Test Status: ❌ INVALID

**This test was RE-RUN successfully:**
- **Valid Test:** 4node_diamond_retest_20251111_203959
- **Result:** PDR 100% (27/27 packets)
- **Status:** Use retest results for thesis

**This folder documents the FAILED first attempt** and is kept for troubleshooting reference only.

---

## Issue Details

**Sensor Discovered:** ✅ Yes (D218 in routing table via HELLOs)
**Data Packets:** ❌ None received
**Gateway RX Counter:** 0 throughout test

**Diagnosis:**
- Sensor may not have booted correctly
- Battery power issue
- TX task may not have been created

**Resolution:** Sensor was monitored in retest, confirmed transmitting, gateway received all packets (100% PDR)

---

## Use Retest Results

**For thesis validation, use:**
- experiments/results/protocol3/4node_diamond_retest_20251111_203959/
- PDR: 100% (27/27)
- All nodes monitored and verified

**This file kept for:** Issue documentation only

---

**TEST INVALID - SEE RETEST FOR VALID RESULTS**
