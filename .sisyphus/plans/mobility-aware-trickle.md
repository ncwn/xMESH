# Mobility-Aware Trickle Implementation

## TL;DR

> **Quick Summary**: Add adaptive scheduling to xMESH that detects network mobility via SNR variance tracking and dynamically adjusts Trickle/detection timers. Enables faster neighbor failure detection in mobile networks while maintaining efficiency in static deployments.
> 
> **Deliverables**:
> - `MobilityDetector` class in xmesh-core (state machine, variance tracking)
> - Runtime setters for `TrickleScheduler` (I_min, I_max)
> - Runtime setters for `GatewayBalancer` (warn/detect thresholds)
> - `DutyCycleBudget` class in firmware/production (1% compliance tracking)
> - Serial commands: `mobility on/off`, `emergency`, `mobility simulate`
> - Integration in main.cpp with backward compatibility (disabled by default)
> 
> **Estimated Effort**: Medium (3-4 focused sessions)
> **Parallel Execution**: YES - 3 waves
> **Critical Path**: Task 1 → Task 4 → Task 5 → Task 6

---

## Context

### Original Request
Implement mobility-aware Trickle scheduling for xMESH LoRa mesh with:
1. Fast neighbor drop detection (reduce from 360s to configurable)
2. RSSI variance-based mobility detection with state machine
3. Adaptive Trickle parameters per state
4. Duty cycle tracking for Thailand 1% compliance
5. Emergency reset trigger via serial command

### Interview Summary
**Key Discussions**:
- Oracle provided state machine design (STATIC/MOBILE/EMERGENCY)
- User confirmed conservative thresholds (variance > 3.0 dB² for MOBILE)
- EMERGENCY is serial-triggered only (no auto-trigger)
- Duty cycle exhaustion: log warning only (no packet drop)
- Aggregate variance across all neighbors (not per-neighbor)
- Include `mobility simulate` command for testing

**Research Findings**:
- TrickleScheduler has NO runtime setters - needs `setIMin()`, `setIMax()`
- GatewayBalancer thresholds are `static constexpr` - needs conversion to instance vars
- ETXTracker stores RSSI estimated from SNR (formula: `snr * 4 - 110`)
- No test infrastructure exists - manual verification via serial commands
- helloReceivedCallback (line 95-106) is ideal hook for RSSI/SNR data feed

### Metis Review
**Identified Gaps** (addressed):
- Use raw SNR for variance (not derived RSSI) - avoids double-smoothing
- Require minimum 5 samples before variance is meaningful
- Add 60s hysteresis between state transitions to prevent flapping
- Include `mobility simulate` command for testing without RF environment
- Boot in STATIC state (no NVS persistence of mobility state)

### Momus Review (High Accuracy)
**Critical Issues Resolved**:
1. **TX Airtime Calculation**: Use simplified LoRa formula based on config.h parameters.
   - Source: `firmware/production/include/config.h:68-72` (SF7, BW125, CR4/5, TX20dBm)
   - Formula (exact, from SX126x datasheet):
     ```
     Tsymbol = 2^SF / BW = 2^7 / 125000 = 1.024ms
     Tpreamble = (Npreamble + 4.25) * Tsymbol = 12.5ms
     payloadSymbols = 8 + max(ceil((8*PL - 4*SF + 28 + 16) / (4*SF)) * (CR+4), 0)
     Tpayload = payloadSymbols * Tsymbol
     Tpacket = Tpreamble + Tpayload
     ```
   - For SF7/BW125/CR5, 20-byte packet: ~50ms
   - Implementation: `estimateAirtimeMs(size_t len)` helper in main.cpp uses this formula with fixed config.h values
2. **Trickle interval gating for MOBILE→STATIC**: Add `bool isAtMaxInterval() const` to TrickleScheduler (returns I_current >= I_max). Pass result to `MobilityDetector.tick(bool trickleAtMax)`.
3. **Setter naming consistency**: Use `setWarningThreshold()`/`setDetectionThreshold()` consistently in GatewayBalancer and in applyMobilityParams().
4. **Variance algorithm clarified**: Use **sliding window** of last 10 raw SNR samples per neighbor. Calculate variance using standard formula: `variance = sum((x - mean)^2) / N`. No Welford - simple and clear.
5. **Neighbor capacity/eviction**: Same as ETXTracker pattern (lib/xmesh-core/src/ETXTracker.cpp:37-55) - max 10 neighbors, LRU eviction when full (evict entry with oldest lastUpdate).

**Second Review Issues Resolved**:
6. **DutyCycleBudget buffer strategy**: 
   - Buffer capacity: 100 entries (max 100 TX events per hour reasonable for mesh)
   - Data structure: `struct AirtimeEntry { uint32_t timestamp; uint16_t durationMs; }` 
   - Circular buffer with `uint8_t head, count`
   - Eviction: Automatic via rolling window - entries older than 1 hour are skipped during sum calculation
   - Time source: `millis()` (ESP32 HAL, wraps at ~49 days - handled)
7. **TX coverage for duty cycle**: Record airtime for ALL transmission paths:
   - Trickle HELLO broadcasts (main.cpp line 537)
   - `send XXXX` command (main.cpp line 331)
   - Both use `estimateAirtimeMs(sizeof(TestPacket))` = ~37ms for 4-byte packet
8. **Timing semantics for state transitions**:
   - `STATIC → MOBILE`: Requires aggregate variance > 3.0 dB² for 3 **consecutive** `tick()` calls (at 60s interval = 180s total)
   - `MOBILE → STATIC`: Requires aggregate variance < 1.5 dB² **continuously** for 120s (tracked via `stableStartTime`) AND `trickleAtMax == true`
   - 60s hysteresis: After ANY state transition, ignore variance checks for 60s (`lastTransitionTime + 60000 > now`)
   - EMERGENCY: 60s hold time tracked separately, transitions to MOBILE after 60s, hysteresis applies

---

## Work Objectives

### Core Objective
Enable xMESH to automatically detect network mobility and adapt Trickle/detection timers for faster response in mobile scenarios while maintaining efficiency in static deployments.

### Concrete Deliverables
- `/Volumes/xMESH/xMESH/lib/xmesh-core/include/xmesh/MobilityDetector.h`
- `/Volumes/xMESH/xMESH/lib/xmesh-core/src/MobilityDetector.cpp`
- Modified `TrickleScheduler.h/cpp` with setters
- Modified `GatewayBalancer.h/cpp` with setters
- `/Volumes/xMESH/xMESH/firmware/production/include/DutyCycleBudget.h`
- `/Volumes/xMESH/xMESH/firmware/production/src/DutyCycleBudget.cpp`
- Modified `main.cpp` with integration and serial commands

### Definition of Done
- [ ] `pio run` compiles with zero errors
- [ ] `mobility on` command enables detection and logs "Mobility detection: ENABLED"
- [ ] `mobility off` command disables and logs "Mobility detection: DISABLED"
- [ ] `emergency` command triggers EMERGENCY state and logs "STATE: EMERGENCY"
- [ ] `status` command shows mobility state (STATIC/MOBILE/EMERGENCY/disabled)
- [ ] `mobility simulate mobile` forces MOBILE state for testing
- [ ] Default boot: mobility disabled (backward compatible)
- [ ] Trickle interval changes on state transition (logged)
- [ ] Detection thresholds change on state transition (logged)

### Must Have
- State machine: STATIC ↔ MOBILE, manual EMERGENCY trigger
- Conservative thresholds: STATIC→MOBILE at variance > 3.0 dB², MOBILE→STATIC at < 1.5 dB² for 120s
- 60s hysteresis between transitions
- Minimum 5 samples before variance calculation
- Aggregate variance across all neighbors
- Parameters per state:
  | State | I_min | I_max | Warn | Detect |
  |-------|-------|-------|------|--------|
  | STATIC | 60s | 600s | 180s | 360s |
  | MOBILE | 20s | 120s | 90s | 180s |
  | EMERGENCY | 10s | 60s | 30s | 90s |
- Duty cycle tracking with warning at 36s/hour
- Serial commands: `mobility on/off`, `emergency`, `mobility simulate <state>`
- Backward compatibility: disabled by default

### Must NOT Have (Guardrails)
- **NO modification to Trickle RFC 6206 algorithm** (doubleInterval, consistency check)
- **NO NVS persistence** of mobility state (boot in STATIC)
- **NO OLED display** updates for mobility state
- **NO per-neighbor** mobility tracking (global state only)
- **NO RSSI filtering** in MobilityDetector (ETXTracker already does EWMA)
- **NO automatic EMERGENCY trigger** (serial command only)
- **NO packet dropping** on duty cycle exhaustion (log warning only)
- **NO WiFi/networking** in MobilityDetector (hardware-agnostic)
- **NO tests** (no test infrastructure exists)
- **NO Kalman filters** or advanced signal processing

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: NO
- **User wants tests**: NO (Manual verification via serial)
- **Framework**: N/A

### Automated Verification (ALWAYS include)

All verification uses serial commands via `pio device monitor`. The agent will:
1. Build firmware with `pio run`
2. Flash device with `pio run -t upload`
3. Send commands via serial and capture output
4. Assert expected patterns in output

**Evidence Requirements**:
- Terminal output captured and compared against expected patterns
- Build logs showing zero errors
- Serial command responses logged

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately - Core Infrastructure):
├── Task 1: Create MobilityDetector class (xmesh-core)
├── Task 2: Add TrickleScheduler runtime setters
└── Task 3: Add GatewayBalancer runtime setters

Wave 2 (After Wave 1 - Firmware Components):
├── Task 4: Create DutyCycleBudget class
└── Task 5: Integrate MobilityDetector in main.cpp

Wave 3 (After Wave 2 - Final Integration):
└── Task 6: Add serial commands and verification

Critical Path: Task 1 → Task 5 → Task 6
Parallel Speedup: ~40% faster than sequential
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 1 | None | 5 | 2, 3 |
| 2 | None | 5 | 1, 3 |
| 3 | None | 5 | 1, 2 |
| 4 | None | 6 | 1, 2, 3 |
| 5 | 1, 2, 3 | 6 | 4 |
| 6 | 4, 5 | None | None (final) |

### Agent Dispatch Summary

| Wave | Tasks | Recommended Dispatch |
|------|-------|-------------------|
| 1 | 1, 2, 3 | `delegate_task(category="quick", load_skills=[], run_in_background=true)` x3 |
| 2 | 4, 5 | `delegate_task(category="unspecified-low", load_skills=[], run_in_background=true)` x2 |
| 3 | 6 | `delegate_task(category="quick", load_skills=[], run_in_background=false)` |

---

## TODOs

### Task 1: Create MobilityDetector Class

**What to do**:
1. Create header file: `lib/xmesh-core/include/xmesh/MobilityDetector.h`
2. Create implementation: `lib/xmesh-core/src/MobilityDetector.cpp`
3. Implement state machine: STATIC, MOBILE, EMERGENCY (enum `MobilityState`)
4. Implement SNR variance tracking using **sliding window** (NOT Welford):
   - Per-neighbor struct `NeighborSNRHistory` with:
     - `uint16_t address`
     - `int8_t snrWindow[10]` - circular buffer of last 10 raw SNR values
     - `uint8_t windowIndex`, `uint8_t windowFilled`
     - `uint32_t lastUpdate`
   - Variance calculation: `sum((x - mean)^2) / N` using stored samples
5. Neighbor tracking: MAX 10 neighbors, LRU eviction (same pattern as ETXTracker.cpp:37-55)
6. Aggregate variance: average variance across all neighbors with ≥5 samples
   - If no neighbors have ≥5 samples, return 0.0 (no mobility detected)
7. State transitions with **precise timing semantics**:
   - `STATIC → MOBILE`: 
     - Condition: aggregate variance > 3.0 dB² for 3 consecutive `tick()` calls
     - Implementation: `highVarianceCount` counter, increment if variance > 3.0, reset to 0 otherwise
     - At 60s tick interval = 180s of high variance triggers transition
   - `MOBILE → STATIC`:
     - Condition: aggregate variance < 1.5 dB² continuously for 120s AND `trickleAtMax == true`
     - Implementation: `stableStartTime` timestamp, set when variance first drops below 1.5
       - Reset `stableStartTime = 0` if variance >= 1.5 at any tick
       - Check `millis() - stableStartTime >= 120000 && trickleAtMax` to transition
   - `ANY → EMERGENCY`: 
     - Trigger: Manual only via `triggerEmergency()` method
     - Sets `emergencyStartTime = millis()` and `state = EMERGENCY`
   - `EMERGENCY → MOBILE`: 
     - Condition: After 60s hold time (`millis() - emergencyStartTime >= 60000`)
     - Checked in `tick()`, transitions automatically
8. 60s hysteresis between any state transitions:
   - After ANY transition: set `lastTransitionTime = millis()`
   - In `tick()`: skip variance checks if `millis() - lastTransitionTime < 60000`
   - Exception: EMERGENCY hold time is separate from hysteresis
9. API:
   ```cpp
   void feedSNR(uint16_t addr, int8_t snr);  // Called on HELLO
   void tick(bool trickleAtMax);              // Periodic check, takes Trickle status
   MobilityState getState() const;
   const char* getStateName() const;          // Returns "STATIC", "MOBILE", or "EMERGENCY"
   bool isEnabled() const;
   void enable();
   void disable();
   void triggerEmergency();
   void simulateState(MobilityState state);   // For testing
   float getAggregateVariance() const;
   ```

**Must NOT do**:
- NO RSSI filtering (use raw SNR values)
- NO automatic EMERGENCY trigger
- NO persistence of state
- NO per-neighbor state tracking
- NO WiFi/networking dependencies

**Recommended Agent Profile**:
- **Category**: `quick`
  - Reason: Single new file creation with clear spec, no complex dependencies
- **Skills**: `[]`
  - No special skills needed - pure C++ implementation

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 1 (with Tasks 2, 3)
- **Blocks**: Task 5
- **Blocked By**: None (can start immediately)

**References**:

**Pattern References**:
- `lib/xmesh-core/include/xmesh/ETXTracker.h:18-39` - LinkMetrics struct pattern for per-neighbor data storage
- `lib/xmesh-core/src/ETXTracker.cpp:25-55` - getLinkMetrics() with LRU eviction pattern for neighbor capacity management
- `lib/xmesh-core/src/ETXTracker.cpp:121-137` - Circular buffer update and iteration pattern
- `lib/xmesh-core/include/xmesh/TrickleScheduler.h:40-44` - State enum pattern

**API/Type References**:
- `lib/xmesh-core/include/xmesh/MeshConfig.h:14-45` - MeshConfig struct pattern for parameter grouping

**WHY Each Reference Matters**:
- ETXTracker.h:18-39 shows how to define per-neighbor data structs with circular buffers
- ETXTracker.cpp:25-55 shows exact LRU eviction logic for when neighbor list is full
- ETXTracker.cpp:121-137 shows circular buffer update pattern to follow
- TrickleScheduler shows state enum pattern used in xmesh-core
- MeshConfig shows how to group parameters into structs

**External References**:
- Standard variance formula: `variance = sum((x - mean)^2) / N`

**Acceptance Criteria**:

```bash
# Agent runs:
cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | grep -E "(error:|Error)"
# Assert: No output (no errors)

# Verify files created:
ls -la /Volumes/xMESH/xMESH/lib/xmesh-core/include/xmesh/MobilityDetector.h
ls -la /Volumes/xMESH/xMESH/lib/xmesh-core/src/MobilityDetector.cpp
# Assert: Both files exist
```

**Evidence to Capture**:
- [ ] Build output showing successful compilation
- [ ] File listing showing new files created

**Commit**: YES (groups with 2, 3)
- Message: `feat(core): add MobilityDetector with state machine and variance tracking`
- Files: `lib/xmesh-core/include/xmesh/MobilityDetector.h`, `lib/xmesh-core/src/MobilityDetector.cpp`
- Pre-commit: `cd firmware/production && pio run`

---

### Task 2: Add TrickleScheduler Runtime Setters

**What to do**:
1. Add to `TrickleScheduler.h` (after line 110):
   ```cpp
   void setIMin(uint32_t ms);
   void setIMax(uint32_t ms);
   bool isAtMaxInterval() const;  // Returns true if I_current == I_max
   ```
2. Implement in `TrickleScheduler.cpp`:
   - `setIMin()`: Update `I_min` member, clamp `I_current` if < new I_min. Log with ESP_LOGI.
   - `setIMax()`: Update `I_max` member, clamp `I_current` if > new I_max. Log with ESP_LOGI.
   - `isAtMaxInterval()`: Return `I_current >= I_max` (simple getter for MobilityDetector)
3. Validate: I_min must be < I_max (log error if invalid, don't apply)
4. Do NOT modify `doubleInterval()`, `shouldTransmit()`, or consistency check logic

**Must NOT do**:
- NO changes to RFC 6206 algorithm logic
- NO changes to `reset()` behavior
- NO changes to suppression logic

**Recommended Agent Profile**:
- **Category**: `quick`
  - Reason: Adding 2 simple setter methods to existing class, minimal changes
- **Skills**: `[]`
  - No special skills needed

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 1 (with Tasks 1, 3)
- **Blocks**: Task 5
- **Blocked By**: None

**References**:

**Pattern References**:
- `lib/xmesh-core/include/xmesh/GatewayBalancer.h:181` - `setIsGateway()` setter pattern
- `lib/xmesh-core/src/TrickleScheduler.cpp:9-23` - Constructor validation pattern

**API/Type References**:
- `lib/xmesh-core/include/xmesh/TrickleScheduler.h:27-30` - Private members to modify (I_min, I_max, I_current)

**WHY Each Reference Matters**:
- GatewayBalancer shows existing setter pattern in xmesh-core
- Constructor shows validation approach (log errors, don't throw)

**Acceptance Criteria**:

```bash
# Agent runs:
cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | grep -E "(error:|Error)"
# Assert: No output (no errors)

# Verify methods exist in header:
grep -n "setIMin\|setIMax\|isAtMaxInterval" /Volumes/xMESH/xMESH/lib/xmesh-core/include/xmesh/TrickleScheduler.h
# Assert: All three methods declared

# Verify implementation exists:
grep -n "TrickleScheduler::setIMin\|TrickleScheduler::setIMax\|TrickleScheduler::isAtMaxInterval" /Volumes/xMESH/xMESH/lib/xmesh-core/src/TrickleScheduler.cpp
# Assert: All three methods implemented
```

**Commit**: YES (groups with 1, 3)
- Message: `feat(core): add runtime setters to TrickleScheduler`
- Files: `lib/xmesh-core/include/xmesh/TrickleScheduler.h`, `lib/xmesh-core/src/TrickleScheduler.cpp`
- Pre-commit: `cd firmware/production && pio run`

---

### Task 3: Add GatewayBalancer Runtime Setters

**What to do**:
1. Convert `static constexpr` thresholds (lines 203-204) to instance variables:
   - `uint32_t detectionThresholdMs` (default 360000)
   - `uint32_t warningThresholdMs` (default 180000)
2. Add setters to header (after line 187):
   ```cpp
   void setDetectionThreshold(uint32_t ms);
   void setWarningThreshold(uint32_t ms);
   uint32_t getDetectionThreshold() const;
   uint32_t getWarningThreshold() const;
   ```
3. Update `monitorNeighborHealth()` to use instance variables instead of constants
4. Implement setters with validation (warn < detect)
5. Initialize in constructor with default values

**Must NOT do**:
- NO changes to neighbor tracking logic
- NO changes to gateway load tracking

**Recommended Agent Profile**:
- **Category**: `quick`
  - Reason: Simple refactor from constexpr to instance variables + setters
- **Skills**: `[]`
  - No special skills needed

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 1 (with Tasks 1, 2)
- **Blocks**: Task 5
- **Blocked By**: None

**References**:

**Pattern References**:
- `lib/xmesh-core/include/xmesh/GatewayBalancer.h:181` - Existing setter pattern
- `lib/xmesh-core/src/GatewayBalancer.cpp:12-18` - Constructor initialization pattern

**API/Type References**:
- `lib/xmesh-core/include/xmesh/GatewayBalancer.h:203-204` - Constants to convert

**WHY Each Reference Matters**:
- Shows existing class structure and where to add new members
- Constructor shows initialization pattern to follow

**Acceptance Criteria**:

```bash
# Agent runs:
cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | grep -E "(error:|Error)"
# Assert: No output (no errors)

# Verify setters exist:
grep -n "setDetectionThreshold\|setWarningThreshold" /Volumes/xMESH/xMESH/lib/xmesh-core/include/xmesh/GatewayBalancer.h
# Assert: Both methods declared

# Verify constants converted to instance variables:
grep -n "detectionThresholdMs\|warningThresholdMs" /Volumes/xMESH/xMESH/lib/xmesh-core/include/xmesh/GatewayBalancer.h
# Assert: Found as private members (not static constexpr)
```

**Commit**: YES (groups with 1, 2)
- Message: `feat(core): add runtime threshold setters to GatewayBalancer`
- Files: `lib/xmesh-core/include/xmesh/GatewayBalancer.h`, `lib/xmesh-core/src/GatewayBalancer.cpp`
- Pre-commit: `cd firmware/production && pio run`

---

### Task 4: Create DutyCycleBudget Class

**What to do**:
1. Create header: `firmware/production/include/DutyCycleBudget.h`
2. Create implementation: `firmware/production/src/DutyCycleBudget.cpp`
3. Data structures:
   ```cpp
   struct AirtimeEntry {
       uint32_t timestamp;   // millis() when TX occurred
       uint16_t durationMs;  // Airtime in milliseconds
   };
   
   class DutyCycleBudget {
   private:
       static constexpr uint8_t MAX_ENTRIES = 100;
       static constexpr uint32_t WINDOW_MS = 3600000;  // 1 hour
       static constexpr uint32_t BUDGET_MS = 36000;    // 36s = 1% of 1 hour
       
       AirtimeEntry entries[MAX_ENTRIES];
       uint8_t head = 0;
       uint8_t count = 0;
       uint32_t cachedTotalMs = 0;
       uint32_t lastCacheUpdate = 0;
   };
   ```
4. API:
   ```cpp
   void recordAirtime(uint32_t durationMs);  // Add entry, wrap if full
   uint32_t getRemainingBudgetMs() const;    // BUDGET_MS - current usage
   float getUsagePercent() const;            // (current usage / BUDGET_MS) * 100
   bool isExhausted() const;                 // current usage >= BUDGET_MS
   void tick();                              // Recalculate cached total, expiring old entries
   ```
5. `recordAirtime()`: Add entry at head, wrap circularly. Log ESP_LOGD.
6. `tick()` / cache update: Iterate all entries, sum only those within last hour. Update cache.
7. Logging:
   - ESP_LOGW when usage exceeds 80% (28.8s)
   - ESP_LOGE when budget exhausted (36s)
8. Do NOT block transmissions - warning only

**Must NOT do**:
- NO packet dropping or blocking
- NO NVS persistence
- NO integration with LoRaMesher (that's Task 5/6)

**Recommended Agent Profile**:
- **Category**: `unspecified-low`
  - Reason: New class creation with clear spec, moderate complexity
- **Skills**: `[]`
  - No special skills needed

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 2 (with Task 5, but independent)
- **Blocks**: Task 6
- **Blocked By**: None (can start in Wave 1 if resources available)

**References**:

**Pattern References**:
- `lib/xmesh-core/src/ETXTracker.cpp:1-50` - Circular buffer and time tracking pattern
- `firmware/production/include/config.h:1-10` - Header file structure pattern

**External References**:
- Thailand NBTC 1% duty cycle: 36 seconds per hour maximum airtime

**WHY Each Reference Matters**:
- ETXTracker shows circular buffer pattern for time-windowed data
- Config.h shows header guard and constexpr patterns

**Acceptance Criteria**:

```bash
# Agent runs:
cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | grep -E "(error:|Error)"
# Assert: No output (no errors)

# Verify files created:
ls -la /Volumes/xMESH/xMESH/firmware/production/include/DutyCycleBudget.h
ls -la /Volumes/xMESH/xMESH/firmware/production/src/DutyCycleBudget.cpp
# Assert: Both files exist
```

**Commit**: YES
- Message: `feat(firmware): add DutyCycleBudget for 1% duty cycle compliance tracking`
- Files: `firmware/production/include/DutyCycleBudget.h`, `firmware/production/src/DutyCycleBudget.cpp`
- Pre-commit: `cd firmware/production && pio run`

---

### Task 5: Integrate MobilityDetector in main.cpp

**What to do**:
1. Add include: `#include <xmesh/MobilityDetector.h>` (after line 15)
2. Add include: `#include "DutyCycleBudget.h"` (after line 15)
3. Add global instance (after line 51):
   ```cpp
   xmesh::MobilityDetector mobilityDetector;
   DutyCycleBudget dutyCycleBudget;
   ```
4. In `helloReceivedCallback()` (line 95-106), add after line 102:
   ```cpp
   mobilityDetector.feedSNR(srcAddr, snr);
   ```
5. In `loop()` (after line 544, inside 60s monitor block):
   ```cpp
   // Mobility detection tick
   if (mobilityDetector.isEnabled()) {
       auto prevState = mobilityDetector.getState();
       mobilityDetector.tick(trickle.isAtMaxInterval());  // Pass Trickle status
       auto newState = mobilityDetector.getState();
       if (newState != prevState) {
           applyMobilityParams(newState);  // Apply new Trickle/GatewayBalancer params
       }
   }
   
   // Duty cycle check
   dutyCycleBudget.tick();
   if (dutyCycleBudget.getUsagePercent() > 80.0f) {
       ESP_LOGW(TAG, "Duty cycle usage: %.1f%% (%.1fs/36s)", 
                dutyCycleBudget.getUsagePercent(),
                dutyCycleBudget.getRemainingBudgetMs() / 1000.0f);
   }
   ```
6. Create helper function `applyMobilityParams(xmesh::MobilityState state)`:
   ```cpp
   void applyMobilityParams(xmesh::MobilityState state) {
       switch (state) {
           case xmesh::MobilityState::STATIC:
               trickle.setIMin(60000);
               trickle.setIMax(600000);
               gatewayBalancer.setWarningThreshold(180000);
               gatewayBalancer.setDetectionThreshold(360000);
               ESP_LOGI(TAG, "Mobility params: STATIC (I_min=60s, I_max=600s)");
               break;
           case xmesh::MobilityState::MOBILE:
               trickle.setIMin(20000);
               trickle.setIMax(120000);
               gatewayBalancer.setWarningThreshold(90000);
               gatewayBalancer.setDetectionThreshold(180000);
               ESP_LOGI(TAG, "Mobility params: MOBILE (I_min=20s, I_max=120s)");
               break;
           case xmesh::MobilityState::EMERGENCY:
               trickle.setIMin(10000);
               trickle.setIMax(60000);
               gatewayBalancer.setWarningThreshold(30000);
               gatewayBalancer.setDetectionThreshold(90000);
               ESP_LOGI(TAG, "Mobility params: EMERGENCY (I_min=10s, I_max=60s)");
               break;
       }
   }
   ```
7. Create helper function `estimateAirtimeMs(size_t payloadLen)` for duty cycle (EXACT formula):
   ```cpp
   // LoRa airtime calculation for SF7, BW125kHz, CR4/5 (from config.h:68-72)
   // Based on SX126x datasheet and RadioLib implementation
   uint32_t estimateAirtimeMs(size_t payloadLen) {
       // Fixed parameters from config.h
       const uint8_t sf = LORA_SPREADING_FACTOR;  // 7
       const float bw = LORA_BANDWIDTH * 1000.0f;  // 125000 Hz
       const uint8_t cr = LORA_CODING_RATE;       // 5 (CR 4/5)
       const uint8_t preamble = 8;                // Default preamble symbols
       const bool explicitHeader = true;
       const bool crc = true;
       const bool ldro = false;  // SF7 doesn't need LDRO
       
       // Symbol time: Tsymbol = 2^SF / BW
       float tsymbol = pow(2.0f, sf) / bw * 1000.0f;  // in ms
       
       // Preamble time
       float tpreamble = (preamble + 4.25f) * tsymbol;
       
       // Payload symbols calculation (from datasheet)
       int16_t de = ldro ? 1 : 0;
       int16_t ih = explicitHeader ? 0 : 1;
       int16_t crcBits = crc ? 16 : 0;
       
       float numerator = 8.0f * payloadLen - 4.0f * sf + 28.0f + crcBits - 20.0f * ih;
       float denominator = 4.0f * (sf - 2.0f * de);
       int16_t symbolCount = 8 + max((int16_t)ceil(numerator / denominator) * (cr), 0);
       
       float tpayload = symbolCount * tsymbol;
       
       return (uint32_t)(tpreamble + tpayload + 0.5f);  // Round to nearest ms
   }
   // For SF7/BW125/CR5, 4-byte packet: ~37ms, 20-byte packet: ~50ms
   ```
8. Add duty cycle recording to ALL TX paths:
   - In Trickle TX section (after line 537):
     ```cpp
     dutyCycleBudget.recordAirtime(estimateAirtimeMs(sizeof(TestPacket)));
     ```
   - In `send XXXX` command (after line 331):
     ```cpp
     dutyCycleBudget.recordAirtime(estimateAirtimeMs(sizeof(TestPacket)));
     ```

**Must NOT do**:
- NO changes to Trickle algorithm
- NO changes to cost calculation
- NO changes to existing serial commands

**Recommended Agent Profile**:
- **Category**: `unspecified-low`
  - Reason: Integration work requiring understanding of existing code structure
- **Skills**: `[]`
  - No special skills needed

**Parallelization**:
- **Can Run In Parallel**: NO (depends on Wave 1)
- **Parallel Group**: Wave 2 (with Task 4, but sequential dependency on 1,2,3)
- **Blocks**: Task 6
- **Blocked By**: Tasks 1, 2, 3

**References**:

**Pattern References**:
- `firmware/production/src/main.cpp:95-106` - helloReceivedCallback integration point
- `firmware/production/src/main.cpp:540-573` - 60s monitor block pattern
- `firmware/production/src/main.cpp:528-538` - TX section pattern

**API/Type References**:
- `lib/xmesh-core/include/xmesh/MobilityDetector.h` - MobilityDetector API (from Task 1)
- `firmware/production/include/DutyCycleBudget.h` - DutyCycleBudget API (from Task 4)
- `firmware/production/include/config.h:68-72` - LoRa parameters (SF7, BW125, CR4/5) for airtime calculation

**WHY Each Reference Matters**:
- helloReceivedCallback is where SNR data is available for mobility detection
- Monitor block is appropriate for periodic mobility tick (60s interval)
- TX section is where airtime should be recorded
- config.h LoRa params define the radio configuration used for airtime estimation

**Acceptance Criteria**:

```bash
# Agent runs:
cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | grep -E "(error:|Error)"
# Assert: No output (no errors)

# Verify includes added:
grep -n "MobilityDetector.h\|DutyCycleBudget.h" /Volumes/xMESH/xMESH/firmware/production/src/main.cpp
# Assert: Both includes present

# Verify global instances:
grep -n "mobilityDetector\|dutyCycleBudget" /Volumes/xMESH/xMESH/firmware/production/src/main.cpp | head -5
# Assert: Both global instances declared

# Verify helper functions created:
grep -n "applyMobilityParams\|estimateAirtimeMs" /Volumes/xMESH/xMESH/firmware/production/src/main.cpp
# Assert: Both functions defined

# Verify correct setter names used:
grep -n "setWarningThreshold\|setDetectionThreshold" /Volumes/xMESH/xMESH/firmware/production/src/main.cpp
# Assert: Found in applyMobilityParams (NOT setWarning/setDetect)
```

**Commit**: NO (combine with Task 6)

---

### Task 6: Add Serial Commands and Verification

**What to do**:
1. Add to `processSerialCommands()` (after line 360):
   ```cpp
   else if (command == "mobility on") {
       mobilityDetector.enable();
       Serial.println("[CMD] Mobility detection: ENABLED");
   }
   else if (command == "mobility off") {
       mobilityDetector.disable();
       // Reset to STATIC params
       applyMobilityParams(xmesh::MobilityState::STATIC);
       Serial.println("[CMD] Mobility detection: DISABLED");
   }
   else if (command == "emergency") {
       mobilityDetector.enable();  // Enable if not already
       mobilityDetector.triggerEmergency();
       applyMobilityParams(xmesh::MobilityState::EMERGENCY);
       Serial.println("[CMD] STATE: EMERGENCY - hold for 60s");
   }
   else if (command.startsWith("mobility simulate ")) {
       String state = command.substring(18);
       if (state == "static") {
           mobilityDetector.simulateState(xmesh::MobilityState::STATIC);
           applyMobilityParams(xmesh::MobilityState::STATIC);
           Serial.println("[CMD] Simulated: STATIC");
       } else if (state == "mobile") {
           mobilityDetector.simulateState(xmesh::MobilityState::MOBILE);
           applyMobilityParams(xmesh::MobilityState::MOBILE);
           Serial.println("[CMD] Simulated: MOBILE");
       } else if (state == "emergency") {
           mobilityDetector.simulateState(xmesh::MobilityState::EMERGENCY);
           applyMobilityParams(xmesh::MobilityState::EMERGENCY);
           Serial.println("[CMD] Simulated: EMERGENCY");
       }
   }
   else if (command == "dutycycle") {
       Serial.printf("[DutyCycle] Usage: %.1f%% (%.1fs / 36s)\\n",
                    dutyCycleBudget.getUsagePercent(),
                    (36000 - dutyCycleBudget.getRemainingBudgetMs()) / 1000.0f);
   }
   ```
2. Update `status` command (after line 316) to include:
   ```cpp
   Serial.printf("Mobility: %s (state: %s)\\n",
                mobilityDetector.isEnabled() ? "enabled" : "disabled",
                mobilityDetector.getStateName());
   Serial.printf("Duty Cycle: %.1f%% (%.1fs remaining)\\n",
                dutyCycleBudget.getUsagePercent(),
                dutyCycleBudget.getRemainingBudgetMs() / 1000.0f);
   ```
3. Update `help` command (after line 358) to include:
   ```cpp
   Serial.println("  mobility on/off - Enable/disable mobility detection");
   Serial.println("  mobility simulate <static|mobile|emergency> - Force state");
   Serial.println("  emergency       - Trigger emergency state");
   Serial.println("  dutycycle       - Show duty cycle usage");
   ```

**Must NOT do**:
- NO changes to existing commands
- NO blocking on duty cycle exhaustion
- NO OLED updates

**Recommended Agent Profile**:
- **Category**: `quick`
  - Reason: Adding serial command handlers following existing pattern
- **Skills**: `[]`
  - No special skills needed

**Parallelization**:
- **Can Run In Parallel**: NO (final integration)
- **Parallel Group**: Wave 3 (sequential after 4, 5)
- **Blocks**: None (final task)
- **Blocked By**: Tasks 4, 5

**References**:

**Pattern References**:
- `firmware/production/src/main.cpp:264-364` - Existing serial command pattern
- `firmware/production/src/main.cpp:306-317` - Status command pattern
- `firmware/production/src/main.cpp:348-359` - Help command pattern

**WHY Each Reference Matters**:
- Serial command pattern shows exact format (if/else if chain, Serial.println output)
- Status command shows where to add mobility info
- Help command shows where to document new commands

**Acceptance Criteria**:

**Build verification:**
```bash
# Agent runs:
cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | grep -E "(error:|Error)"
# Assert: No output (no errors)
```

**Serial command verification (using interactive_bash/tmux):**
```
# Flash and monitor:
cd /Volumes/xMESH/xMESH/firmware/production && pio run -t upload && sleep 5

# Test mobility on:
echo "mobility on" > /dev/tty.usbserial-*
# Assert output contains: "Mobility detection: ENABLED"

# Test status:
echo "status" > /dev/tty.usbserial-*
# Assert output contains: "Mobility:" with state

# Test mobility simulate:
echo "mobility simulate mobile" > /dev/tty.usbserial-*
# Assert output contains: "Simulated: MOBILE"

# Test emergency:
echo "emergency" > /dev/tty.usbserial-*
# Assert output contains: "STATE: EMERGENCY"

# Test mobility off:
echo "mobility off" > /dev/tty.usbserial-*
# Assert output contains: "Mobility detection: DISABLED"

# Test help includes new commands:
echo "help" > /dev/tty.usbserial-*
# Assert output contains: "mobility on/off"
```

**Evidence to Capture**:
- [ ] Build output showing successful compilation
- [ ] Serial output showing all commands work
- [ ] Status command showing mobility state
- [ ] Screenshot of full verification sequence

**Commit**: YES (includes Task 5 changes)
- Message: `feat(firmware): integrate mobility detection with serial commands`
- Files: `firmware/production/src/main.cpp`
- Pre-commit: `cd firmware/production && pio run`

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1+2+3 | `feat(core): add MobilityDetector and runtime setters for Trickle/GatewayBalancer` | lib/xmesh-core/* | `pio run` |
| 4 | `feat(firmware): add DutyCycleBudget for 1% compliance tracking` | firmware/production/include/DutyCycleBudget.h, src/DutyCycleBudget.cpp | `pio run` |
| 5+6 | `feat(firmware): integrate mobility detection with serial commands` | firmware/production/src/main.cpp | `pio run && pio run -t upload` |

---

## Success Criteria

### Verification Commands
```bash
# Build must succeed
cd /Volumes/xMESH/xMESH/firmware/production && pio run
# Expected: BUILD SUCCESSFUL

# After flash, serial commands must work
# (requires hardware connected)
echo "mobility on" | pio device monitor --baud 115200 --filter send
# Expected: "[CMD] Mobility detection: ENABLED"

echo "status" | pio device monitor --baud 115200 --filter send
# Expected: "Mobility: enabled (state: STATIC)"

echo "mobility simulate mobile" | pio device monitor --baud 115200 --filter send
# Expected: "[CMD] Simulated: MOBILE"
# Expected: Trickle log showing I_min=20s, I_max=120s
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent
- [ ] Build succeeds with zero errors
- [ ] All serial commands respond correctly
- [ ] Default boot shows mobility disabled
- [ ] State transitions logged appropriately
- [ ] Backward compatibility verified (mobility off = original behavior)
