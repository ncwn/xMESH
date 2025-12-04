# Analysis: LoRaMesher 1-Hop Preference Problem
**Test:** sensor-cold-start_Problem-with-LoRaMesher-1hop
**Log:** node1_20251119_141200.log
**Date:** November 19, 2025
**Duration:** ~14 minutes (840s)

---

## 🔍 **Problem Discovered**

This test revealed a fundamental limitation in LoRaMesher's distance-vector routing that prevented Protocol 3 from proving superiority over Protocol 2.

---

## 📊 **Test Configuration**

```
Sensor: BB94 (Plum Condo Phase 1, Level 8)
Distance: ~935m to relay, 5 floors to gateways
Radio: SF9, 20 dBm TX power
Network: 4 nodes (1 sensor, 1 relay, 2 gateways)
```

---

## ⚠️ **Observed Behavior**

### Routing Table (Throughout Test):
```
Addr   Via    Hops  Role  Cost
------|------|------|------|------
D218 | D218 |    1 | 01 | 1.45  ← Direct path (weak signal!)
6674 | 6674 |    1 | 01 | 1.46  ← Direct path (weak signal!)
8154 | 8154 |    1 | 00 | 1.36  ← Relay (good signal)
```

### Signal Quality:
```
D218: RSSI=-131 to -137 dBm, SNR=-10 to -13 dB  (EXTREMELY WEAK!)
6674: RSSI=-135 dBm, SNR=-11 to -14 dB          (EXTREMELY WEAK!)
8154: RSSI=-99 to -107 dBm, SNR=+2 to -5 dB     (GOOD - 30 dB better!)
```

### Packet Transmission:
```
All packets sent direct:
TX: Seq=1 to Gateway=D218 (Hops=1)
TX: Seq=2 to Gateway=D218 (Hops=1)
TX: Seq=3 to Gateway=D218 (Hops=1)
...
TX: Seq=12 to Gateway=6674 (Hops=1)
```

**NO multi-hop routing used** despite relay having much better signal! ❌

---

## 🔬 **Root Cause Analysis**

### Issue 1: LoRaMesher Only Keeps Lowest Hop-Count Route

**What happens:**
1. Sensor hears Gateway D218 directly (weak, RSSI=-131 dBm)
   - Stores: `D218 | D218 | 1` (1-hop direct)

2. Sensor hears Relay forwarding D218's HELLO
   - Receives: `D218 via 8154 (2 hops)`
   - **LoRaMesher REJECTS this** because 2 > 1 (keeps lowest hop-count)

3. Cost function never sees the 2-hop option to compare!

**Code location:** `src/services/RoutingTableService.cpp:240-243`
```cpp
if (calculateMaximumMetricOfRoutingTable() < node->metric) {
    ESP_LOGW(LM_TAG, "metric higher than maximum, not adding route");
    return;  // ← 2-hop route discarded here!
}
```

### Issue 2: Cost Function Can't Override Hop-Count

Even though Protocol 3 calculates sophisticated costs:
```
Direct path cost:  1.45 (1 hop, terrible signal)
Relay path cost:   2.30 (2 hops, good signal)  ← WOULD BE BETTER!
```

**The relay path was never stored in the routing table!**

**Result:** Protocol 3 behaves identically to Protocol 2 (hop-count routing)

---

## 💡 **W5 Load Sharing Did Work!**

**Despite 1-hop limitation, W5 successfully balanced between gateways:**

```
Gateway D218: load=1.0 pkt/min → bias=+1.00
Gateway 6674: load=0.0 pkt/min → bias=-1.00

Cost adjustment:
D218: 1.43 + 1.00 = 2.43 (avoid!)
6674: 1.43 - 1.00 = 0.43 (prefer!)

[14:23:10.001] [W5] Load-biased gateway selection: 6674
TX: Seq=10 to Gateway=6674  ← Switched! ✅
TX: Seq=11 to Gateway=6674  ← Load balanced! ✅
```

**W5 worked correctly** - it just operated within the 1-hop limitation.

---

## 📋 **Implications for Thesis**

### What This Test Proved:

❌ **Protocol 3 NOT superior to Protocol 2** (in this test)
- Both use hop-count routing (1-hop paths only)
- Cost function calculated but couldn't override LoRaMesher
- Multi-hop routing unavailable despite better quality

✅ **W5 load sharing works** (within same hop-count)
- Successfully balanced between two 1-hop gateways
- Bias calculation correct (±1.00)
- Gateway switching validated

### Why This Was a Problem:

**Research claim:** "Protocol 3 uses multi-metric cost to make better routing decisions"

**Reality in this test:** Protocol 3 couldn't choose multi-hop because LoRaMesher discarded the option

**Impact:** Cannot claim superiority over Protocol 2 without fixing this!

---

## 🔧 **Required Fix**

### Modification Needed:

**File:** `src/services/RoutingTableService.cpp`

**Change:** Allow cost-based route replacement even when hop-count increases

**Implementation:**
```cpp
// In addNodeToRoutingTable():
if (node->metric > existingRoute->metric && costCallback != nullptr) {
    // Normally reject, BUT check if cost is better
    if (newCost < existingCost * 0.8) {
        // Replace with better-quality multi-hop route
        REPLACE_ROUTE();  ✅
    }
}
```

**This fix enables Protocol 3 to fulfill its research promise!**

---

## 📊 **Test Summary**

| Feature | Status | Notes |
|---------|--------|-------|
| **Multi-hop discovery** | ⚠️ BLOCKED | 2-hop routes rejected by LoRaMesher |
| **Cost calculation** | ✅ WORKING | Costs calculated correctly |
| **W5 load sharing** | ✅ WORKING | Balanced within 1-hop routes |
| **Trickle suppression** | ✅ WORKING | 25-33% efficiency |
| **PM sensor** | ✅ WORKING | 3-11 µg/m³ readings |
| **GPS** | ✅ WORKING | 4-7 satellites |
| **Weak signal handling** | ❌ FAILED | Used direct despite RSSI=-135 dBm |

**Overall:** 🟡 **Partial success - W5 works, but multi-hop blocked**

---

## 🎯 **Outcome**

**This test identified a critical gap in Protocol 3 implementation:**

The protocol had all the components (cost function, metrics, calculations) but couldn't USE them due to LoRaMesher's hop-count filtering.

**Resolution:** Modified LoRaMesher to allow cost-based route replacement (Nov 19, 2025)

**Follow-up test:** `sensor-cold-start_20251119_145041` validated the fix works! ✅

---

## 📝 **Key Takeaways**

1. **W5 load sharing works independently** of hop-count issues
2. **Cost calculation was correct** all along (just not applied to route selection)
3. **LoRaMesher modification required** for true cost-based routing
4. **Weak link penalty needed** to make multi-hop competitive

**This test was valuable** - it exposed the implementation gap and led to the critical fix!

---

## 🔗 **Related Tests**

- **sensor-cold-start_Weak-link/** - First attempt at weak link penalty (0.5, insufficient)
- **sensor-cold-start_20251119_145041/** - Final solution (penalty=1.5, route replacement enabled)

**Test progression:** Problem identified → Fix attempted → Solution validated ✅
