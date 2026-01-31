# LoRaMesher Fork Modifications

**Base Version:** LoRaMesher (commit snapshot in `.LoRaMesher-OG/`)  
**Last Updated:** 2025-01-31  
**Purpose:** Document all modifications made to the upstream LoRaMesher library for xMESH integration

## Overview

xMESH maintains a modified fork of LoRaMesher in `src/`. The original upstream code is preserved in `.LoRaMesher-OG/` for diff comparison and future merge operations.

**Key Modifications:**
1. Added `LM_GOD_MODE` compile flag to expose internal APIs
2. Added cost calculation callbacks for multi-metric routing
3. Added HELLO received callbacks for Trickle/ETX integration
4. Added route removal API for xMESH extensions
5. Thread-safety improvements

## File-by-File Changes

### BuildOptions.h

**Location:** `src/BuildOptions.h`

**Change:** Added `LM_GOD_MODE` compile flag

```diff
+ // xMESH: Expose internal APIs for advanced routing hooks
+ #ifndef LM_GOD_MODE
+ #define LM_GOD_MODE 1
+ #endif
```

**Rationale:** LoRaMesher's internal routing table and packet services are normally hidden. `LM_GOD_MODE` (replacing the original `LM_TESTING` flag) exposes these internals so xMESH can:
- Hook into route selection via callbacks
- Access routing table for snapshots
- Inject custom cost calculations

**Impact:** No behavioral change when flag is 0. When enabled, internal headers are accessible.

---

### RoutingTableService.h

**Location:** `src/services/RoutingTableService.h`

**Changes:**

1. **Added callback types:**
```cpp
// xMESH: Callback for custom route cost calculation
using CostCalculationCallback = std::function<float(
    uint8_t metric,      // Original hop count
    uint16_t via,        // Next-hop address
    uint16_t dest,       // Destination address
    int16_t rssi,        // Received signal strength
    int8_t snr           // Signal-to-noise ratio
)>;

// xMESH: Callback when HELLO packet is received
using HelloReceivedCallback = std::function<void(
    uint16_t from,       // Sender address
    uint8_t numNodes,    // Nodes in routing table
    int16_t rssi,        // RSSI of received HELLO
    int8_t snr           // SNR of received HELLO
)>;
```

2. **Added callback setters:**
```cpp
void setCostCalculationCallback(CostCalculationCallback cb);
void setHelloReceivedCallback(HelloReceivedCallback cb);
```

3. **Added route removal method:**
```cpp
void removeRoute(uint16_t address);
```

4. **Private members:**
```cpp
CostCalculationCallback costCallback_ = nullptr;
HelloReceivedCallback helloCallback_ = nullptr;
```

**Rationale:**
- `CostCalculationCallback`: Enables xMESH CostRouter to inject multi-metric cost (RSSI, SNR, ETX, gateway bias) into route selection
- `HelloReceivedCallback`: Enables xMESH modules (TrickleScheduler, ETXTracker, GatewayBalancer) to react to neighbor advertisements
- `removeRoute()`: Allows xMESH to proactively remove failed neighbors

---

### RoutingTableService.cpp

**Location:** `src/services/RoutingTableService.cpp`

**Changes:**

1. **Callback invocation in HELLO processing:**
```cpp
void RoutingTableService::processRoute(/* params */) {
    // ... existing code ...
    
    // xMESH: Notify listeners of HELLO reception
    if (helloCallback_) {
        helloCallback_(from, numNodes, rssi, snr);
    }
    
    // ... rest of processing ...
}
```

2. **Cost-based route comparison:**
```cpp
void RoutingTableService::processReceivedNode(/* params */) {
    // Original: if (newHops < existingHops) updateRoute();
    
    // xMESH: Use cost callback if available
    if (costCallback_) {
        float newCost = costCallback_(newHops, via, dest, rssi, snr);
        float currentCost = costCallback_(existingHops, currentVia, dest, 
                                          existingRssi, existingSNR);
        
        // Hysteresis: new route must be 15% better
        if (newCost < currentCost * 0.85f) {
            updateRoute(dest, via, newHops);
        }
    } else {
        // Fallback to original hop-count comparison
        if (newHops < existingHops) {
            updateRoute(dest, via, newHops);
        }
    }
}
```

3. **Route removal implementation:**
```cpp
void RoutingTableService::removeRoute(uint16_t address) {
    LockGuard lock(routingTableMutex);
    auto it = routingTable.find(address);
    if (it != routingTable.end()) {
        routingTable.erase(it);
    }
}
```

4. **Callback setters:**
```cpp
void RoutingTableService::setCostCalculationCallback(CostCalculationCallback cb) {
    costCallback_ = cb;
}

void RoutingTableService::setHelloReceivedCallback(HelloReceivedCallback cb) {
    helloCallback_ = cb;
}
```

**Rationale:**
- Route selection is the core integration point for xMESH's advanced routing
- 0.85 hysteresis factor prevents route flapping in marginal cases
- Callback architecture allows clean separation: LoRaMesher handles mesh mechanics, xMESH handles route intelligence

---

### LoraMesher.h

**Location:** `src/LoraMesher.h`

**Changes:**

1. **Thread-safe message flag:**
```cpp
// Original: bool hasReceivedMessage = false;
volatile bool hasReceivedMessage = false;  // xMESH: Thread-safe flag
```

2. **Public route deletion:**
```cpp
#ifdef LM_GOD_MODE
void deleteRoute(uint16_t address);  // xMESH: Expose route removal
#endif
```

**Rationale:**
- `volatile` ensures ISR and task-context reads are consistent
- `deleteRoute()` wrapper provides public API for xMESH modules to remove failed neighbors

---

### LoraMesher.cpp

**Location:** `src/LoraMesher.cpp`

**Changes:**

1. **deleteRoute implementation:**
```cpp
#ifdef LM_GOD_MODE
void LoraMesher::deleteRoute(uint16_t address) {
    routingTableService->removeRoute(address);
}
#endif
```

2. **Removed LM_TESTING conditionals:**
```diff
- #ifdef LM_TESTING
-     // Test-specific code
- #endif
```
Replaced with `LM_GOD_MODE` guards where internal access is needed.

**Rationale:**
- `LM_TESTING` was for unit tests only; `LM_GOD_MODE` is for production extensions
- Cleaner conditional compilation with single feature flag

---

## Integration Points

### How xMESH Hooks Into LoRaMesher

```
┌─────────────────────────────────────────────────────────────────┐
│                        main.cpp (xMESH)                          │
├─────────────────────────────────────────────────────────────────┤
│  1. LoraMesher::getInstance().begin()                           │
│  2. routingTableService->setCostCalculationCallback(...)        │
│  3. routingTableService->setHelloReceivedCallback(...)          │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                  RoutingTableService (modified)                  │
├─────────────────────────────────────────────────────────────────┤
│  On HELLO received:                                             │
│    1. helloCallback_(from, numNodes, rssi, snr)                 │
│       → TrickleScheduler::onHelloReceived()                     │
│       → ETXTracker::recordReception()                           │
│       → GatewayBalancer::updateNeighbor()                       │
│                                                                 │
│  On route comparison:                                           │
│    2. costCallback_(metric, via, dest, rssi, snr)              │
│       → CostRouter::calculateCost()                             │
│       → Returns: weighted sum of all metrics                    │
└─────────────────────────────────────────────────────────────────┘
```

### Callback Flow Example

```cpp
// In main.cpp setup:
auto& radio = LoraMesher::getInstance();
auto* rts = radio.getRoutingTableService();

// Set cost callback
rts->setCostCalculationCallback([](uint8_t hops, uint16_t via, 
                                   uint16_t dest, int16_t rssi, int8_t snr) {
    return CostRouter::calculateCost(hops, via, dest, rssi, snr);
});

// Set HELLO callback
rts->setHelloReceivedCallback([](uint16_t from, uint8_t nodes, 
                                  int16_t rssi, int8_t snr) {
    TrickleScheduler::onConsistentHello();
    ETXTracker::recordReception(from, currentSeq++);  // BUG: should use packet seq
    GatewayBalancer::updateNeighborHealth(from);
    MobilityDetector::feedSNR(from, snr);
});
```

---

## Upgrade Considerations

### Merging Upstream Changes

When pulling updates from upstream LoRaMesher:

1. **Check for conflicts in:**
   - `BuildOptions.h` - May have new flags
   - `RoutingTableService.h/cpp` - Core modification area
   - `LoraMesher.h/cpp` - May have new public API

2. **Preserve these modifications:**
   - `LM_GOD_MODE` flag definition
   - All callback types and members
   - `removeRoute()` and `deleteRoute()` methods
   - `volatile hasReceivedMessage`

3. **Test after merge:**
   ```bash
   cd firmware/production
   pio test -e native  # Run unit tests
   pio run             # Verify build
   ```

### Diff Command

To see current modifications:
```bash
diff -r .LoRaMesher-OG/src src --exclude="*.o" --exclude="*.d"
```

### Risk Areas

| File | Risk | Notes |
|------|------|-------|
| `RoutingTableService.cpp` | HIGH | Core route processing modified |
| `LoraMesher.h` | MEDIUM | Public API additions |
| `BuildOptions.h` | LOW | Additive only |

---

## Behavioral Changes Summary

| Aspect | Original LoRaMesher | xMESH Fork |
|--------|---------------------|------------|
| Route selection | Hop count only | Multi-metric cost (configurable) |
| Route hysteresis | None | 15% improvement required |
| HELLO notifications | None | Callbacks for external modules |
| Route removal | Internal only | Public API via `deleteRoute()` |
| Thread safety | Basic | `volatile` for ISR-shared flags |
| Internal access | Hidden | Exposed via `LM_GOD_MODE` |

---

## Maintenance Checklist

When modifying fork:
- [ ] Update this document
- [ ] Preserve `.LoRaMesher-OG/` unchanged
- [ ] Run unit tests after changes
- [ ] Verify callback behavior with integration tests
- [ ] Document any new `LM_GOD_MODE` dependencies
