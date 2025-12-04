# Trickle HELLO Implementation - Protocol 3

## Overview

Protocol 3 now has **true Trickle-controlled HELLO transmission** (RFC 6206) with adaptive 60-600s intervals, implemented entirely in the firmware folder without modifying the main LoRaMesher library.

## Implementation Strategy

Instead of modifying LoRaMesher's core library code, we:

1. **Suspend** LoRaMesher's fixed 120s HELLO task
2. **Create** our own Trickle-controlled HELLO task in `trickle_hello.h`
3. **Use** the same LoRaMesher APIs (PacketService, RoutingTableService) to send HELLOs

**Files Modified (all in firmware/3_gateway_routing/src/):**
- `trickle_hello.h` (NEW) - Trickle HELLO task implementation
- `main.cpp` - Integration code (include header, call init)

**Files NOT Modified:**
- `src/` (LoRaMesher library) - Unchanged!

## How It Works

### Initialization ([main.cpp:1155-1157](firmware/3_gateway_routing/src/main.cpp#L1155-L1157))

```cpp
if (trickleTimer.isEnabled()) {
    initTrickleHello();  // Suspend LoRaMesher HELLO, start Trickle HELLO
}
```

### Trickle HELLO Task ([trickle_hello.h](firmware/3_gateway_routing/src/trickle_hello.h))

```
Loop every 1 second:
├─ Ask Trickle: shouldTransmit()?
│  ├─ NO: Continue waiting
│  └─ YES: Send HELLO packet
│     ├─ Get routing table: getAllNetworkNodes()
│     ├─ Create RoutePacket: createRoutingPacket()
│     └─ Queue for send: setPackedForSend()
└─ Trickle manages interval doubling:
   60s → 120s → 240s → 480s → 600s (max)
```

### Trickle Algorithm ([main.cpp:153-329](firmware/3_gateway_routing/src/main.cpp#L153-L329))

**Adaptive Interval:**
- Starts at **I_min = 60s**
- Doubles each interval: 60→120→240→480→**600s (max)**
- Resets to 60s on topology changes

**Suppression (future enhancement):**
- If heard ≥k HELLOs: suppress transmission
- Currently: k=1, but `heardConsistent()` not called yet
- Next step: Add HELLO reception feedback

## Expected Behavior

### Stable Network (3 nodes: Sensor, Relay, Gateway)

```
t=0s:     Node boots, I=60s
          Next TX scheduled at t=random(30-60s) ≈ 45s

t=45s:    HELLO sent (interval 1)
          [TrickleHELLO] Sending HELLO - interval=60.0s

t=60s:    Interval expires, double to I=120s
          [Trickle] DOUBLE - I=120.0s

t=105s:   HELLO sent (interval 2)
          [TrickleHELLO] Sending HELLO - interval=120.0s

t=120s:   Interval expires, double to I=240s
          [Trickle] DOUBLE - I=240.0s

t=210s:   HELLO sent (interval 3)
          [TrickleHELLO] Sending HELLO - interval=240.0s

...continues to I_max=600s
```

### With Topology Change

```
t=300s:   New node joins, routing table changes
          [TRICKLE] Topology change detected - resetting to I_min
          [Trickle] RESET - I=60.0s

t=330s:   HELLO sent (fast convergence)
          [TrickleHELLO] Sending HELLO - interval=60.0s
```

## Testing

### Build and Flash

```bash
cd firmware/3_gateway_routing

# Flash Node 1 (Sensor)
./flash_node.sh 1 /dev/cu.usbserial-0001

# Flash Node 3 (Relay)
./flash_node.sh 3 /dev/cu.usbserial-5

# Flash Node 5 (Gateway)
./flash_node.sh 5 /dev/cu.usbserial-7
```

### Monitor Serial Output

```bash
# Watch for these log messages:
pio device monitor --port /dev/cu.usbserial-0001 --baud 115200
```

**Expected logs:**
```
[TrickleHELLO] Initializing Trickle-controlled HELLO system
[TrickleHELLO] ✅ Suspended LoRaMesher's fixed 120s HELLO task
[TrickleHELLO] ✅ Started Trickle HELLO task (60-600s adaptive)
✅ TRICKLE ACTIVE - Adaptive HELLO intervals (60-600s)
   Initial: 60.0s, Max: 600.0s, k=1
   Overhead reduction: 80-97% expected vs fixed 120s

[Trickle] Started - I=60.0s
[Trickle] RESET - I=60.0s, next TX in 45.3s
[TrickleHELLO] Task started - replacing LoRaMesher fixed HELLO
[TrickleHELLO] Max nodes per packet: 13

[TrickleHELLO] Sending HELLO - interval=60.0s
[Trickle] TRANSMIT - count=1, interval=60.0s
[Trickle] DOUBLE - I=120.0s, next TX in 95.7s

[TrickleHELLO] Sending HELLO - interval=120.0s
[Trickle] TRANSMIT - count=2, interval=120.0s
[Trickle] DOUBLE - I=240.0s, next TX in 180.2s

...
```

### Measure HELLO Reduction

**Protocol 2 (baseline):**
```bash
# Monitor for 15 minutes, count HELLOs
# Expected: 15 min × (60/120) = 7.5 HELLOs per node
# Total: 7.5 × 3 nodes = ~22 HELLOs
```

**Protocol 3 (Trickle):**
```bash
# Monitor for 15 minutes, count HELLOs
# Expected at I_max=600s: 15 min × (60/600) = 1.5 HELLOs per node
# During ramp-up: ~3-4 HELLOs per node (60,120,240,480,600s intervals)
# Total: 3-4 × 3 nodes = ~9-12 HELLOs
# Reduction: 50-60% even during ramp-up, 80-90% at steady state
```

## Configuration

All settings in [config.h](firmware/3_gateway_routing/src/config.h#L73-L77):

```cpp
#define TRICKLE_IMIN_MS     60000   // 60s minimum interval
#define TRICKLE_IMAX_MS     600000  // 600s maximum interval
#define TRICKLE_K           1       // Suppression threshold
#define TRICKLE_ENABLED     true    // Enable/disable Trickle
```

**To disable Trickle** (revert to 120s fixed):
```cpp
#define TRICKLE_ENABLED     false
```

## Future Enhancements

### 1. HELLO Reception Feedback (Suppression)

Currently, Trickle provides **adaptive intervals** but not **suppression**.

To add suppression, detect when HELLOs are received:

```cpp
// In evaluateRoutingTableCosts() when route refreshed without metric change:
if (currentVia == history->via && currentCost ≈ history->cost) {
    // Route refreshed = consistent HELLO heard
    onHelloReceived(destAddr);  // Calls trickleTimer.heardConsistent()
}
```

With this, Trickle will suppress HELLOs if k≥1 neighbors already sent.

### 2. Lower I_min for Faster Convergence

```cpp
#define TRICKLE_IMIN_MS     30000   // 30s for 2x faster convergence
```

### 3. Variable k Based on Network Density

```cpp
uint8_t k = min(routingTableSize() / 2, 3);  // More neighbors = higher k
```

## Verification

### Check Trickle is Active

```bash
# Serial output should show:
✅ TRICKLE ACTIVE - Adaptive HELLO intervals (60-600s)
[TrickleHELLO] ✅ Started Trickle HELLO task
[TrickleHELLO] ✅ Suspended LoRaMesher's fixed 120s HELLO task
```

### Measure Interval Growth

```bash
# HELLOs should be at increasing intervals:
grep "Sending HELLO" node1.log | awk '{print $1}' | while read -r time; do
    if [ -n "$prev" ]; then
        echo "Interval: $((time - prev))s"
    fi
    prev=$time
done

# Expected output:
# Interval: 60s
# Interval: 120s
# Interval: 240s
# Interval: 480s
# Interval: 600s (steady state)
```

## Troubleshooting

### "Could not find LoRaMesher HELLO task to suspend"

**Cause:** Task name mismatch or timing issue

**Fix:** Increase delay before suspension:
```cpp
// In trickle_hello.h:initTrickleHello()
vTaskDelay(500 / portTICK_PERIOD_MS);  // Increase from 100ms
```

### HELLOs Still Every 120s

**Check 1:** Verify TRICKLE_ENABLED=true in config.h

**Check 2:** Check serial logs for initialization messages

**Check 3:** Ensure trickle_hello.h is included in main.cpp

### Routing Table Not Building

**Cause:** HELLO packets malformed or not sent

**Debug:**
```cpp
// In trickleHelloTask(), add logging:
Serial.printf("[DEBUG] Creating RoutePacket: %d nodes\n", numOfNodes);
Serial.printf("[DEBUG] Queued for send, priority=%d\n", DEFAULT_PRIORITY + 4);
```

## Summary

✅ **Implemented:** Trickle adaptive HELLO (60-600s)
✅ **Location:** Entirely in firmware/3_gateway_routing/src/
✅ **LoRaMesher:** Unchanged (clean separation)
✅ **Expected reduction:** 80-97% HELLO overhead vs fixed 120s

⏳ **Future:** Add heardConsistent() feedback for suppression

---

**Status:** Ready for hardware testing! 🚀
