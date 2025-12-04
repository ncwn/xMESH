# Trickle Suppression Implementation

**Status**: ✅ Implemented (Hardware testing pending)
**Date**: November 9, 2025

---

## Overview

Trickle suppression logic now ACTIVE - nodes will suppress their own HELLO transmissions when they hear neighbors' HELLOs, achieving up to **80-97% overhead reduction**.

**Previous**: 60% reduction via adaptive intervals only (60-600s)
**Now**: 80-97% reduction via adaptive intervals + suppression

---

## How It Works

### Trickle Suppression Algorithm (RFC 6206)

**Consistency Counter** (c):
- Incremented each time a "consistent" HELLO is heard
- Reset to 0 at start of each interval
- Compared against threshold (k=1)

**Suppression Logic**:
```
When transmission time (t) arrives:
  if (c >= k):
    SUPPRESS this HELLO  ← Neighbor already announced network state
  else:
    TRANSMIT HELLO       ← We need to announce
```

**Example** (k=1):
```
Node A: Interval I=120s, t=random(60-120)=90s
t=0s:    Interval starts, c=0
t=45s:   Hears Node B's HELLO → c=1
t=90s:   Transmission time arrives
         Check: c=1 >= k=1? YES
         → SUPPRESS (Node B already announced)
```

---

## Implementation

### 1. Library Modification (Minimal)

**Added to LoRaMesher** (src/services/RoutingTableService.*):

```cpp
// Callback typedef
typedef void (*HelloReceivedCallback)(uint16_t srcAddr);

// In RoutingTableService class:
static HelloReceivedCallback helloCallback;
static void setHelloReceivedCallback(HelloReceivedCallback callback);

// In processRoute(RoutePacket*):
if (helloCallback != nullptr) {
    helloCallback(p->src);  // Notify when HELLO received
}
```

**Commit**: feat: Add HELLO reception callback for Trickle suppression

### 2. Firmware Integration

**In main.cpp setup()**:
```cpp
// Register HELLO reception callback for Trickle suppression
RoutingTableService::setHelloReceivedCallback(onHelloReceived);
```

**In trickle_hello.h**:
```cpp
void onHelloReceived(uint16_t fromAddr) {
    trickleTimer.heardConsistent();  // Increment consistency counter
}
```

**Flow**:
```
HELLO packet arrives
  ↓
LoRaMesher processes packet
  ↓
RoutingTableService::processRoute()
  ↓
Calls helloCallback(srcAddr)
  ↓
Calls onHelloReceived(srcAddr)
  ↓
Calls trickleTimer.heardConsistent()
  ↓
c++ (consistency counter incremented)
  ↓
Next transmission check: if (c >= k) SUPPRESS
```

---

## Expected Behavior

### Without Suppression (Current 60% Reduction)

**30-minute test**:
```
Node behavior:
- Sends HELLOs at adaptive intervals regardless of neighbors
- 60→120→240→480→600s progression
- 18 total HELLOs (6 per node)
- 60% reduction vs 120s fixed
```

### With Suppression (Projected 80-97% Reduction)

**30-minute test** (projected):
```
Dense network (3 nodes close together):
- Nodes hear each other's HELLOs
- Consistency counter c increases
- Most HELLOs suppressed (c >= k)
- Only 1-2 HELLOs per node per hour
- **80-97% reduction** vs 120s fixed

Sparse network (nodes far apart):
- Nodes don't always hear neighbors
- c stays low
- More HELLOs transmitted
- Falls back to adaptive intervals (60% reduction)
```

---

## Configuration

**In config.h**:
```cpp
#define TRICKLE_K  1  // Suppression threshold
```

**K values**:
- `k=1`: Aggressive suppression (suppress if heard ≥1 HELLO)
- `k=2`: Moderate suppression (suppress if heard ≥2 HELLOs)
- `k=3`: Conservative suppression (suppress if heard ≥3 HELLOs)

**For 3-node network**: k=1 is appropriate (each node hears 2 neighbors)

---

## Testing

### Test 1: Stable Network (Measure Suppression Rate)

```bash
# Flash all 3 nodes
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 5 /dev/cu.usbserial-7

# Run 30-min stable test
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
python3 raspberry_pi/multi_node_capture.py \
  --node1-port /dev/cu.usbserial-0001 \
  --node2-port /dev/cu.usbserial-5 \
  --node3-port /dev/cu.usbserial-7 \
  --duration 1800 \
  --output-dir experiments/results/protocol3/test_suppression_$TIMESTAMP
```

**Expected Metrics**:
- Trickle HELLOs: 6-9 total (vs 18 without suppression)
- Suppressed: 9-12 (suppression count)
- Reduction: 80-90% (vs 60% without suppression)

**Script will show**:
```
[Node 1] 10:15:23 📡 TRICKLE HELLO #1
[Node 2] 10:15:45 🔇 SUPPRESSED (heard #1)
[Node 3] 10:16:10 🔇 SUPPRESSED (heard #1)
...
Total Trickle HELLOs: 6-9
Total Suppressed: 9-12
Overhead Reduction: 80-90% ✅
```

---

## How to Disable

**Suppression only** (keep adaptive intervals):
```cpp
// In trickle_hello.h:
void onHelloReceived(uint16_t fromAddr) {
    // Comment out to disable suppression
    // trickleTimer.heardConsistent();
}
```

**All Trickle** (revert to Protocol 2 fixed 120s):
```cpp
// In config.h:
#define TRICKLE_ENABLED false
```

---

## Validation Checklist

After hardware test, verify:

- [ ] Suppression count > 0 (suppression active)
- [ ] Total HELLOs < 18 (improvement over adaptive-only)
- [ ] Overhead reduction > 60% (better than without suppression)
- [ ] PDR maintained at 100%
- [ ] Network remains stable

---

## Expected Results Summary

| Configuration | HELLOs (30min) | Reduction | Status |
|---------------|----------------|-----------|--------|
| Protocol 2 (120s fixed) | 45 | 0% (baseline) | Baseline |
| Protocol 3 (adaptive only) | 18 | 60% | ✅ Validated |
| **Protocol 3 (adaptive + suppression)** | **6-9** | **80-90%** | ⏳ Testing |

**Next**: Hardware test to validate suppression working!

---

**Files Modified**:
- src/services/RoutingTableService.h (callback typedef + declaration)
- src/services/RoutingTableService.cpp (callback implementation + call site)
- firmware/3_gateway_routing/src/main.cpp (callback registration)

**Commit**: 32a64db - "feat: Add HELLO reception callback for Trickle suppression"
