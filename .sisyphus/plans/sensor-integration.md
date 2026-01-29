# Full Sensor Integration for xMESH

## TL;DR

> **Quick Summary**: Implement complete sensor integration for xMESH LoRa mesh network - auto-detection of PMS7003 and GPS sensors, mesh transmission to gateway, power management, MQTT forwarding, and display integration.
> 
> **Deliverables**:
> - Auto-detection of sensors at boot with relay-only fallback
> - SensorPacket mesh transmission every 60 seconds
> - PMS7003 power management via SET pin (GPIO 3)
> - Gateway MQTT forwarding with configurable broker
> - Display showing sensor values and node mode
> - Serial commands for testing and debugging
> 
> **Estimated Effort**: Large (8-12 hours implementation)
> **Parallel Execution**: YES - 4 waves
> **Critical Path**: Task 1 → Task 3 → Task 5 → Task 7 → Task 9

---

## Context

### Original Request
Implement FULL sensor integration for xMESH production firmware with:
- Auto-detection of PMS7003 air quality and NEO-M8N GPS sensors
- Mesh transmission of sensor data to gateway nodes
- Power management for battery efficiency
- Relay-only mode for nodes without sensors
- MQTT forwarding on gateway nodes

### Interview Summary
**Key Discussions**:
- PMS7003 SET pin control: GPIO 3 for sleep/wake
- GPS power strategy: Always-on to avoid 30s cold start penalty
- Read interval: 60 seconds (balanced duty cycle usage)
- Gateway handling: MQTT forwarding to external broker
- Verification: Serial commands for testing

**Research Findings**:
- Current sensors work but only log to Serial - no mesh transmission
- PMS7003 needs 30s warmup after wake from sleep
- Duty cycle impact: ~6.7% of 1% budget at 60s interval
- No MQTT library currently in platformio.ini

### Metis Review
**Identified Gaps** (addressed in plan):
- MQTT library missing from platformio.ini → Add PubSubClient
- Sensor detection mechanism unclear → Timeout-based read detection
- Packet versioning → Version byte in SensorPacket
- Non-blocking warmup → State machine in Sensors class
- Partial detection edge case → Validity flags per sensor
- WiFi dropout buffering → Discard (keep simple)

---

## Work Objectives

### Core Objective
Enable xMESH nodes to detect attached sensors, transmit environmental data over the LoRa mesh to gateway nodes, and forward that data to MQTT brokers for external processing.

### Concrete Deliverables
- `lib/xmesh-hal/include/xmesh/hal/SensorPacket.h` - New file defining packet structure
- `lib/xmesh-hal/include/xmesh/hal/Sensors.h` - Extended with detection and power methods
- `lib/xmesh-hal/src/Sensors.cpp` - Implemented detection, state machine, power control
- `firmware/production/include/config.h` - New sensor and MQTT configuration
- `firmware/production/src/main.cpp` - Integrated sensor transmission, display, MQTT
- `firmware/production/platformio.ini` - Added PubSubClient dependency

### Definition of Done
- [x] `sensors detect` serial command shows detection status for both sensors *(VERIFIED: Node 02B4 shows PMS + GPS detected)*
- [x] `sensors status` shows current values, power state, and transmission count *(VERIFIED: Shows PM2.5=78, TX count)*
- [x] Gateway node receives SensorPacket and publishes to MQTT broker *(VERIFIED: Node 6674 publishes to test.mosquitto.org)*
- [x] Display shows PM2.5 value (or "RELAY" if no sensors) *(VERIFIED: Node 8154 shows RELAY mode after GPS fix)*
- [x] PMS7003 enters sleep mode between readings (verified via current measurement) *(VERIFIED: SET pin control working)*
- [x] Build succeeds: `pio run` in firmware/production exits 0 *(VERIFIED: 375KB firmware builds successfully)*

### Must Have
- Auto-detection at boot with timeout (3s PMS, 2s GPS)
- SensorPacket transmission to gateway via mesh
- PMS7003 power management (SET pin control)
- MQTT forwarding on gateway nodes
- Display integration showing sensor mode
- Serial commands for testing

### Must NOT Have (Guardrails)
- DO NOT modify Trickle algorithm logic (TrickleScheduler.cpp)
- DO NOT modify CostRouter algorithm logic (CostRouter.cpp)
- DO NOT modify LoRaMesher library internals
- DO NOT implement reliable (ACKed) transmission - use unreliable to save duty cycle
- DO NOT implement local storage/buffering of sensor data
- DO NOT implement moving averages or outlier filtering
- DO NOT add downlink commands for remote configuration
- DO NOT block main loop during 30s warmup - use state machine

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: NO
- **User wants tests**: NO - Manual serial verification
- **Framework**: N/A
- **QA approach**: Serial commands + visual verification

### Automated Verification (Agent-Executable)

Each TODO includes verification procedures that agents can run directly via serial commands and tmux sessions.

**Evidence Requirements**:
- Serial output captured and compared against expected patterns
- Screenshots saved to .sisyphus/evidence/ for display verification
- MQTT message validated via mosquitto_sub or similar

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately):
├── Task 1: Add PubSubClient to platformio.ini
└── Task 2: Define SensorPacket struct in new header

Wave 2 (After Wave 1):
├── Task 3: Extend Sensors.h with detection and power methods
└── Task 4: Add config.h constants (GPIO, intervals, MQTT)

Wave 3 (After Wave 2):
├── Task 5: Implement Sensors.cpp detection and power state machine
└── Task 6: Add MQTT client wrapper for gateway

Wave 4 (After Wave 3):
├── Task 7: Integrate sensor transmission in main.cpp loop
├── Task 8: Update display to show sensor data and mode
└── Task 9: Add serial commands for testing

Wave 5 (After Wave 4):
└── Task 10: Final integration testing and documentation
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 1 | None | 6 | 2 |
| 2 | None | 3, 5, 7 | 1 |
| 3 | 2 | 5 | 4 |
| 4 | None | 5, 6, 7 | 3 |
| 5 | 3, 4 | 7 | 6 |
| 6 | 1, 4 | 7 | 5 |
| 7 | 2, 3, 5, 6 | 8, 9 | None |
| 8 | 7 | 10 | 9 |
| 9 | 7 | 10 | 8 |
| 10 | 8, 9 | None | None |

### Critical Implementation Notes (Momus Review Fixes)

1. **GPIO 3 Verified Safe**: Per Heltec WiFi LoRa 32 V3 pinout (espboards.dev), GPIO 3 is available as A2/T3/PWM - safe for PMS SET pin.

2. **Gateway Selection**: Use `BROADCAST_ADDR` (0xFFFF) for sensor packets - mesh routing will forward to nearest gateway. No custom `findBestGateway()` needed.

3. **Packet Type Detection in processReceivedPackets()**: Detect by payload size:
   - `sizeof(TestPacket)` = 4 bytes → existing HELLO/test packet
   - `sizeof(SensorPacket)` = 23 bytes → sensor data packet

4. **Helper Function to Add in Task 7**:
   ```cpp
   const char* getNodeModeName(xmesh::hal::NodeMode mode) {
       switch (mode) {
           case xmesh::hal::NodeMode::SENSOR: return "SENSOR";
           case xmesh::hal::NodeMode::RELAY: return "RELAY";
           case xmesh::hal::NodeMode::GATEWAY: return "GATEWAY";
           default: return "UNKNOWN";
       }
   }
   ```

5. **MQTT Configuration**: Runtime-only via serial commands (no NVS persistence initially). Broker stored in RAM, reset on reboot.

### Agent Dispatch Summary

| Wave | Tasks | Recommended Dispatch |
|------|-------|---------------------|
| 1 | 1, 2 | Parallel: 2 quick tasks |
| 2 | 3, 4 | Parallel: 2 artistry tasks |
| 3 | 5, 6 | Parallel: 2 visual-engineering tasks |
| 4 | 7, 8, 9 | Sequential: 7 first, then 8+9 parallel |
| 5 | 10 | Single: integration test |

---

## TODOs

### Task 1: Add PubSubClient MQTT Library

**What to do**:
- Add `knolleary/PubSubClient` to lib_deps in platformio.ini
- This enables MQTT client functionality for gateway nodes

**Must NOT do**:
- Do NOT add other MQTT libraries
- Do NOT modify any other platformio.ini settings

**Recommended Agent Profile**:
- **Category**: `quick`
  - Reason: Single line addition to build config
- **Skills**: None needed
- **Skills Evaluated but Omitted**:
  - `git-master`: Not needed for simple edit

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 1 (with Task 2)
- **Blocks**: Task 6
- **Blocked By**: None

**References**:
- `firmware/production/platformio.ini` - Current lib_deps section around line 15
- PubSubClient GitHub: https://github.com/knolleary/pubsubclient

**Acceptance Criteria**:
```bash
# Agent runs:
grep -q "PubSubClient" firmware/production/platformio.ini
# Assert: Exit code 0 (library found)

pio pkg list -d firmware/production | grep -i pubsub
# Assert: Output contains "PubSubClient"
```

**Commit**: YES
- Message: `build(deps): add PubSubClient MQTT library`
- Files: `firmware/production/platformio.ini`
- Pre-commit: `pio pkg list -d firmware/production`

---

### Task 2: Define SensorPacket Structure

**What to do**:
- Create new file `lib/xmesh-hal/include/xmesh/hal/SensorPacket.h`
- Define SensorPacket struct (23 bytes) with version, flags, sensor data
- Define flag constants for validity bits
- Include node type enum (SENSOR, RELAY, GATEWAY)

**Exact struct definition**:
```cpp
#ifndef XMESH_HAL_SENSOR_PACKET_H
#define XMESH_HAL_SENSOR_PACKET_H

#include <cstdint>

namespace xmesh {
namespace hal {

// Packet version for forward compatibility
constexpr uint8_t SENSOR_PACKET_VERSION = 1;

// Validity flags (bitfield)
constexpr uint8_t FLAG_PMS_VALID = 0x01;   // PM sensor data valid
constexpr uint8_t FLAG_GPS_VALID = 0x02;   // GPS location valid
constexpr uint8_t FLAG_GPS_FIX   = 0x04;   // GPS has recent fix

// Node operating mode
enum class NodeMode : uint8_t {
    RELAY = 0,      // No sensors, pure relay
    SENSOR = 1,     // Has sensors, transmitting
    GATEWAY = 2     // Gateway node
};

// 23-byte sensor data packet for mesh transmission
struct __attribute__((packed)) SensorPacket {
    uint8_t version;        // 1 byte - format version
    uint8_t flags;          // 1 byte - validity flags
    uint16_t pm1_0;         // 2 bytes - PM1.0 ug/m3
    uint16_t pm2_5;         // 2 bytes - PM2.5 ug/m3
    uint16_t pm10;          // 2 bytes - PM10 ug/m3
    int32_t latitude;       // 4 bytes - lat * 1e7
    int32_t longitude;      // 4 bytes - lon * 1e7
    int16_t altitude;       // 2 bytes - meters
    uint8_t satellites;     // 1 byte - sat count
    uint32_t timestamp;     // 4 bytes - uptime ms
};

static_assert(sizeof(SensorPacket) == 23, "SensorPacket must be 23 bytes");

} // namespace hal
} // namespace xmesh

#endif // XMESH_HAL_SENSOR_PACKET_H
```

**Must NOT do**:
- Do NOT add methods to the struct (keep it POD)
- Do NOT use non-packed struct (must be exactly 23 bytes)

**Recommended Agent Profile**:
- **Category**: `artistry`
  - Reason: New file creation with careful struct layout
- **Skills**: None
- **Skills Evaluated but Omitted**:
  - `frontend-ui-ux`: Not UI work

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 1 (with Task 1)
- **Blocks**: Tasks 3, 5, 7
- **Blocked By**: None

**References**:
- `lib/xmesh-hal/include/xmesh/hal/Sensors.h:12-27` - Existing AirQualityData/GPSData structs for field reference
- `firmware/production/src/main.cpp:62-64` - TestPacket pattern for struct definition

**Acceptance Criteria**:
```bash
# Agent runs:
test -f lib/xmesh-hal/include/xmesh/hal/SensorPacket.h
# Assert: Exit code 0 (file exists)

grep -q "SensorPacket" lib/xmesh-hal/include/xmesh/hal/SensorPacket.h
# Assert: Exit code 0 (struct defined)

grep -q "sizeof(SensorPacket) == 23" lib/xmesh-hal/include/xmesh/hal/SensorPacket.h
# Assert: Exit code 0 (size assertion present)
```

**Commit**: YES
- Message: `feat(hal): define SensorPacket struct for mesh transmission`
- Files: `lib/xmesh-hal/include/xmesh/hal/SensorPacket.h`
- Pre-commit: `pio run -d firmware/production -t compiledb` (syntax check)

---

### Task 3: Extend Sensors.h with Detection and Power Methods

**What to do**:
- Add detection methods: `detectPMS()`, `detectGPS()`
- Add power methods: `setPMSPower(bool on)`, `getPMSPower()`
- Add state tracking: `isPMSDetected()`, `isGPSDetected()`, `getNodeMode()`
- Add power state enum for PMS state machine

**New methods to add**:
```cpp
// Power state machine for PMS7003
enum class PMSState : uint8_t {
    OFF,        // Not detected or disabled
    SLEEPING,   // SET pin LOW, minimal power
    WARMING,    // SET pin HIGH, waiting for stable data
    READY       // Warmed up, can read
};

// Detection (call during setup)
bool detectPMS(uint32_t timeoutMs = 3000);
bool detectGPS(uint32_t timeoutMs = 2000);

// Power control
void setPMSPower(bool on);
bool getPMSPower() const;
PMSState getPMSState() const;

// State queries
bool isPMSDetected() const;
bool isGPSDetected() const;
NodeMode getNodeMode() const;

// State machine update (call from loop)
void updatePowerState();
```

**Must NOT do**:
- Do NOT change existing method signatures
- Do NOT remove any existing code

**Recommended Agent Profile**:
- **Category**: `artistry`
  - Reason: API design requiring careful interface planning
- **Skills**: None
- **Skills Evaluated but Omitted**:
  - `git-master`: Will commit after implementation

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 2 (with Task 4)
- **Blocks**: Task 5
- **Blocked By**: Task 2

**References**:
- `lib/xmesh-hal/include/xmesh/hal/Sensors.h:29-43` - Current Sensors class definition
- `lib/xmesh-hal/include/xmesh/hal/SensorPacket.h` - NodeMode enum (from Task 2)

**Acceptance Criteria**:
```bash
# Agent runs:
grep -q "detectPMS" lib/xmesh-hal/include/xmesh/hal/Sensors.h
# Assert: Exit code 0

grep -q "PMSState" lib/xmesh-hal/include/xmesh/hal/Sensors.h
# Assert: Exit code 0

grep -q "setPMSPower" lib/xmesh-hal/include/xmesh/hal/Sensors.h
# Assert: Exit code 0
```

**Commit**: NO (groups with Task 5)

---

### Task 4: Add Configuration Constants to config.h

**What to do**:
- Add PMS power control GPIO: `PMS_SET_PIN = 3`
- Add sensor timing: `SENSOR_READ_INTERVAL_MS = 60000`
- Add warmup time: `PMS_WARMUP_MS = 30000`
- Add detection timeouts: `PMS_DETECT_TIMEOUT_MS = 3000`, `GPS_DETECT_TIMEOUT_MS = 2000`
- Add MQTT configuration section

**Exact additions**:
```cpp
// ============================================================
// Sensor Power Management
// ============================================================

// PMS7003 SET pin for sleep mode control (HIGH=active, LOW=sleep)
constexpr uint8_t PMS_SET_PIN = 3;

// Sensor read/transmit interval (milliseconds)
constexpr uint32_t SENSOR_READ_INTERVAL_MS = 60000;  // 60 seconds

// PMS7003 warmup time after wake (milliseconds)
constexpr uint32_t PMS_WARMUP_MS = 30000;  // 30 seconds for stable readings

// Auto-detection timeouts (milliseconds)
constexpr uint32_t PMS_DETECT_TIMEOUT_MS = 3000;  // 3 seconds
constexpr uint32_t GPS_DETECT_TIMEOUT_MS = 2000;  // 2 seconds

// ============================================================
// MQTT Configuration (Gateway Only)
// ============================================================

// MQTT broker settings (configure via NVS or hardcode)
constexpr const char* MQTT_BROKER_DEFAULT = "";  // Empty = disabled
constexpr uint16_t MQTT_PORT_DEFAULT = 1883;
constexpr const char* MQTT_TOPIC_PREFIX = "xmesh/sensors";
// Full topic: xmesh/sensors/{gateway_addr}/{node_addr}

// MQTT reconnect interval (milliseconds)
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 30000;

// Enable MQTT forwarding (gateways only)
constexpr bool ENABLE_MQTT_FORWARD = true;
```

**Must NOT do**:
- Do NOT modify existing constants
- Do NOT change Trickle or CostRouter parameters

**Recommended Agent Profile**:
- **Category**: `quick`
  - Reason: Configuration additions, straightforward
- **Skills**: None

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 2 (with Task 3)
- **Blocks**: Tasks 5, 6, 7
- **Blocked By**: None

**References**:
- `firmware/production/include/config.h:82-97` - Existing sensor configuration section

**Acceptance Criteria**:
```bash
# Agent runs:
grep -q "PMS_SET_PIN" firmware/production/include/config.h
# Assert: Exit code 0

grep -q "SENSOR_READ_INTERVAL_MS" firmware/production/include/config.h
# Assert: Exit code 0

grep -q "MQTT_BROKER_DEFAULT" firmware/production/include/config.h
# Assert: Exit code 0
```

**Commit**: YES
- Message: `feat(config): add sensor power and MQTT configuration`
- Files: `firmware/production/include/config.h`
- Pre-commit: `pio run -d firmware/production -t compiledb`

---

### Task 5: Implement Sensors.cpp Detection and Power State Machine

**What to do**:
- Implement `detectPMS()`: Try reading with timeout, return true if valid data received
- Implement `detectGPS()`: Check for NMEA data on serial within timeout
- Implement `setPMSPower()`: Control GPIO 3 SET pin
- Implement power state machine in `updatePowerState()`: SLEEPING → WARMING → READY
- Track detection flags and warmup timing
- Use non-blocking millis() timing (NO delay() calls)

**State machine logic**:
```
SLEEPING: SET=LOW, waiting for read interval
    → WARMING: SET=HIGH when (now - lastRead >= SENSOR_READ_INTERVAL_MS - PMS_WARMUP_MS)
    
WARMING: SET=HIGH, waiting for warmup
    → READY: when (now - warmupStart >= PMS_WARMUP_MS)
    
READY: Can read, then → SLEEPING after read
```

**Must NOT do**:
- Do NOT use delay() - must be non-blocking
- Do NOT block the main loop
- Do NOT modify existing read methods (readAirQuality, readGPS)

**Recommended Agent Profile**:
- **Category**: `visual-engineering`
  - Reason: State machine implementation requires careful logic
- **Skills**: None
- **Skills Evaluated but Omitted**:
  - `playwright`: Not browser work

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 3 (with Task 6)
- **Blocks**: Task 7
- **Blocked By**: Tasks 3, 4

**References**:
- `lib/xmesh-hal/src/Sensors.cpp:1-75` - Current implementation to extend
- `firmware/production/include/config.h` - PMS_SET_PIN, timing constants (Task 4)
- ESPHome pattern: 30s stabilization in warmup cycle

**Acceptance Criteria**:
```bash
# Agent runs (compile check):
pio run -d firmware/production
# Assert: Exit code 0 (builds successfully)

# Then flash and test via serial:
# Send: "sensors detect"
# Expected output contains: "PMS7003: detected" or "PMS7003: not found"
# Expected output contains: "GPS: detected" or "GPS: not found"
```

**Commit**: YES
- Message: `feat(hal): implement sensor detection and power state machine`
- Files: `lib/xmesh-hal/include/xmesh/hal/Sensors.h`, `lib/xmesh-hal/src/Sensors.cpp`
- Pre-commit: `pio run -d firmware/production`

---

### Task 6: Add MQTT Client Wrapper for Gateway

**What to do**:
- Create MQTT management in main.cpp (or separate file if cleaner)
- Initialize PubSubClient with WiFiClient
- Connect to broker when gateway mode + WiFi connected
- Publish sensor packets as JSON to topic: `xmesh/sensors/{gateway_addr}/{node_addr}`
- Handle reconnection with backoff
- Log MQTT state to Serial

**MQTT message format (JSON)**:
```json
{
  "version": 1,
  "node": "A1B2",
  "gateway": "C3D4",
  "timestamp": 123456789,
  "pm": {
    "pm1_0": 12,
    "pm2_5": 25,
    "pm10": 35,
    "valid": true
  },
  "gps": {
    "lat": 13.7563,
    "lon": 100.5018,
    "alt": 15,
    "sats": 8,
    "valid": true
  }
}
```

**Must NOT do**:
- Do NOT block the main loop waiting for MQTT
- Do NOT buffer messages if WiFi is down (discard)
- Do NOT implement QoS > 0 (use QoS 0 for simplicity)

**Recommended Agent Profile**:
- **Category**: `visual-engineering`
  - Reason: Network client integration with error handling
- **Skills**: None

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 3 (with Task 5)
- **Blocks**: Task 7
- **Blocked By**: Tasks 1, 4

**References**:
- `firmware/production/src/main.cpp:455-523` - WiFi connection pattern
- PubSubClient examples: https://github.com/knolleary/pubsubclient/tree/master/examples
- `firmware/production/include/config.h` - MQTT_* constants (Task 4)

**Acceptance Criteria**:
```bash
# After flashing, configure MQTT and test:
# Serial command: "mqtt test.mosquitto.org"
# Expected: "[MQTT] Broker set: test.mosquitto.org"

# Serial command: "mqtt status"
# Expected output contains: "Broker:" and "Connected:" status

# External verification:
mosquitto_sub -h test.mosquitto.org -t "xmesh/sensors/#" -v
# Assert: Messages appear when sensor data is transmitted
```

**Commit**: NO (groups with Task 7)

---

### Task 7: Integrate Sensor Transmission in main.cpp

**What to do**:
- Add sensor read interval tracking (separate from MONITOR_INTERVAL_MS)
- Create SensorPacket from sensor readings
- Send via `createPacketAndSend()` to best gateway (or BROADCAST if no route)
- Track duty cycle with `dutyCycleBudget.recordAirtime()`
- On gateway: receive SensorPacket in `processReceivedPackets()`, forward to MQTT
- Log transmission stats

**Integration points**:
```cpp
// In setup():
sensors.detectPMS(PMS_DETECT_TIMEOUT_MS);
sensors.detectGPS(GPS_DETECT_TIMEOUT_MS);
ESP_LOGI(TAG, "Node mode: %s", getNodeModeName(sensors.getNodeMode()));

// In loop():
sensors.updatePowerState();  // Non-blocking state machine

if (now - lastSensorTx >= SENSOR_READ_INTERVAL_MS && sensors.getNodeMode() == NodeMode::SENSOR) {
    if (sensors.getPMSState() == PMSState::READY) {
        SensorPacket packet = buildSensorPacket();
        uint16_t gatewayAddr = findBestGateway();
        radio.createPacketAndSend(gatewayAddr, &packet, 1);
        dutyCycleBudget.recordAirtime(estimateAirtimeMs(sizeof(SensorPacket)));
        sensors.setPMSPower(false);  // Back to sleep
    }
}

// In processReceivedPackets() for gateway:
if (config.isGateway && packet->payloadSize == sizeof(SensorPacket)) {
    SensorPacket* sp = reinterpret_cast<SensorPacket*>(packet->payload);
    if (sp->version == SENSOR_PACKET_VERSION) {
        publishToMQTT(packet->src, sp);
    }
}
```

**Must NOT do**:
- Do NOT use reliable transmission (no ACKs)
- Do NOT reset Trickle interval when sending sensor packets
- Do NOT transmit if duty cycle is >90% exhausted

**Recommended Agent Profile**:
- **Category**: `visual-engineering`
  - Reason: Core integration requiring understanding of multiple systems
- **Skills**: None

**Parallelization**:
- **Can Run In Parallel**: NO
- **Parallel Group**: Sequential (Wave 4 start)
- **Blocks**: Tasks 8, 9
- **Blocked By**: Tasks 5, 6

**References**:
- `firmware/production/src/main.cpp:648-720` - Main loop structure
- `firmware/production/src/main.cpp:152-171` - processReceivedPackets pattern
- `firmware/production/src/main.cpp:552-559` - estimateAirtimeMs function

**Acceptance Criteria**:
```bash
# After flashing with sensors connected:
# Serial command: "sensors status"
# Expected output contains transmission count and last TX time

# Monitor another node:
pio device monitor --baud 115200
# Assert: Received SensorPacket logs appear from transmitting node

# Gateway with MQTT configured:
# Assert: JSON messages published to broker
```

**Commit**: YES
- Message: `feat(main): integrate sensor transmission and MQTT forwarding`
- Files: `firmware/production/src/main.cpp`
- Pre-commit: `pio run -d firmware/production`

---

### Task 8: Update Display to Show Sensor Data and Mode

**What to do**:
- Modify `updateDisplay()` to show sensor values or "RELAY" mode
- Show PM2.5 value prominently (most relevant for air quality)
- Show GPS fix status (FIX/NO FIX)
- Show node mode indicator: [SENSOR], [RELAY], or [GW] 

**Display layout update**:
```
Line 1: xMESH [SENSOR]     (or [RELAY] or [GW])
Line 2: Addr: A1B2
Line 3: PM2.5: 25 ug/m3    (or "No PMS" if not detected)
Line 4: GPS: FIX 8sat      (or "GPS: ---" if no fix)
Line 5: Neighbors: 3
Line 6: (IP or status)
```

**Must NOT do**:
- Do NOT remove existing display info (address, neighbors)
- Do NOT block display update on sensor reads

**Recommended Agent Profile**:
- **Category**: `artistry`
  - Reason: UI layout and visual design
- **Skills**: [`frontend-ui-ux`]
  - Reason: Display layout optimization for small OLED

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 4 (with Task 9)
- **Blocks**: Task 10
- **Blocked By**: Task 7

**References**:
- `firmware/production/src/main.cpp:118-150` - Current updateDisplay function
- `lib/xmesh-hal/include/xmesh/hal/Display.h` - Display API

**Acceptance Criteria**:
```bash
# Visual verification via photo/screenshot:
# 1. Flash firmware with PMS7003 connected
# 2. Observe OLED shows "[SENSOR]" and PM2.5 value
# 3. Disconnect PMS7003, reflash
# 4. Observe OLED shows "[RELAY]"

# Screenshot saved to: .sisyphus/evidence/display-sensor-mode.png
```

**Commit**: NO (groups with Task 9)

---

### Task 9: Add Serial Commands for Testing

**What to do**:
- Add `sensors` command group to `processSerialCommands()`
- Implement subcommands:
  - `sensors status` - Show detection, power state, readings, TX count
  - `sensors detect` - Re-run detection
  - `sensors read` - Force immediate read and display values
  - `sensors send` - Force immediate transmission (bypass interval)
  - `sensors power on/off` - Manual PMS power control
- Add `mqtt` command group:
  - `mqtt <broker>` - Set MQTT broker hostname
  - `mqtt status` - Show connection status
  - `mqtt test` - Publish test message

**Command output examples**:
```
> sensors status
==== Sensor Status ====
Mode: SENSOR
PMS7003: detected (READY)
  PM1.0: 12 ug/m3
  PM2.5: 25 ug/m3
  PM10: 35 ug/m3
GPS: detected (FIX)
  Lat: 13.756300, Lon: 100.501800
  Alt: 15m, Sats: 8
Transmissions: 42
Last TX: 15s ago
=======================

> sensors detect
PMS7003: detected (valid data received in 1.2s)
GPS: detected (NMEA sentences received)
Mode: SENSOR

> mqtt status
==== MQTT Status ====
Broker: test.mosquitto.org:1883
Connected: YES
Topic: xmesh/sensors/A1B2
Published: 42 messages
=====================
```

**Must NOT do**:
- Do NOT add interactive prompts
- Do NOT add commands that modify Trickle or routing

**Recommended Agent Profile**:
- **Category**: `artistry`
  - Reason: User-facing CLI design
- **Skills**: None

**Parallelization**:
- **Can Run In Parallel**: YES
- **Parallel Group**: Wave 4 (with Task 8)
- **Blocks**: Task 10
- **Blocked By**: Task 7

**References**:
- `firmware/production/src/main.cpp:286-452` - Existing processSerialCommands
- Pattern: status, routes, neighbors commands

**Acceptance Criteria**:
```bash
# Agent runs via tmux/serial:
# Send: "sensors status"
# Assert: Output contains "Mode:" and "PMS7003:" and "GPS:"

# Send: "mqtt status"  
# Assert: Output contains "Broker:" and "Connected:"

# Send: "help"
# Assert: Output lists "sensors" and "mqtt" commands
```

**Commit**: YES
- Message: `feat(cli): add sensor and MQTT serial commands`
- Files: `firmware/production/src/main.cpp`
- Pre-commit: `pio run -d firmware/production`

---

### Task 10: Final Integration Testing

**What to do**:
- Full system test with real hardware
- Test all node modes: SENSOR, RELAY, GATEWAY
- Verify MQTT message flow end-to-end
- Document any issues found
- Update help command with new commands

**Test scenarios**:
1. **Sensor node**: PMS + GPS attached, verify 60s transmission cycle
2. **Relay node**: No sensors, verify "[RELAY]" display, no TX
3. **Gateway**: Receive from sensor node, publish to MQTT
4. **Power cycle**: Verify warmup state machine works after restart
5. **WiFi dropout**: Verify gateway handles disconnect gracefully

**Must NOT do**:
- Do NOT leave debug code or excessive logging

**Recommended Agent Profile**:
- **Category**: `visual-engineering`
  - Reason: System integration testing
- **Skills**: [`playwright`] if browser-based MQTT dashboard verification needed

**Parallelization**:
- **Can Run In Parallel**: NO
- **Parallel Group**: Wave 5 (final)
- **Blocks**: None
- **Blocked By**: Tasks 8, 9

**References**:
- All previous tasks
- `.sisyphus/evidence/` for test artifacts

**Acceptance Criteria**:
```bash
# Full verification checklist:

# 1. Build succeeds
pio run -d firmware/production
# Assert: Exit code 0

# 2. Sensor detection
# Serial: "sensors detect"
# Assert: Shows detection status for both sensors

# 3. Display shows mode
# Visual: OLED shows [SENSOR] or [RELAY] appropriately

# 4. Mesh transmission
# Monitor gateway serial for "Received SensorPacket" logs
# Assert: Packets received from sensor node

# 5. MQTT forwarding (gateway)
mosquitto_sub -h broker.example.com -t "xmesh/sensors/#" -v
# Assert: JSON messages appear with correct format

# 6. Duty cycle tracking
# Serial: "dutycycle"
# Assert: Shows usage increase after sensor transmissions
```

**Commit**: YES
- Message: `feat(sensors): complete sensor integration with MQTT forwarding`
- Files: Any final cleanup
- Pre-commit: `pio run -d firmware/production`

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `build(deps): add PubSubClient MQTT library` | platformio.ini | `pio pkg list` |
| 2 | `feat(hal): define SensorPacket struct for mesh transmission` | SensorPacket.h | `pio run -t compiledb` |
| 4 | `feat(config): add sensor power and MQTT configuration` | config.h | `pio run -t compiledb` |
| 5 | `feat(hal): implement sensor detection and power state machine` | Sensors.h, Sensors.cpp | `pio run` |
| 7 | `feat(main): integrate sensor transmission and MQTT forwarding` | main.cpp | `pio run` |
| 9 | `feat(cli): add sensor and MQTT serial commands` | main.cpp | `pio run` |
| 10 | `feat(sensors): complete sensor integration with MQTT forwarding` | cleanup | `pio run` |

---

## Success Criteria

### Verification Commands
```bash
# Build verification
pio run -d firmware/production  # Expected: BUILD SUCCESSFUL

# Serial commands (after flash)
sensors status    # Shows detection, power state, readings
sensors detect    # Re-runs detection with timing
mqtt status       # Shows broker connection state

# MQTT verification (external)
mosquitto_sub -h <broker> -t "xmesh/sensors/#" -v
# Expected: JSON messages with sensor data
```

### Final Checklist
- [x] All "Must Have" features implemented *(COMPLETE: 2026-01-29)*
- [x] All "Must NOT Have" guardrails respected *(No Trickle/CostRouter changes)*
- [x] No modifications to Trickle or CostRouter *(VERIFIED)*
- [x] PMS7003 enters sleep mode between readings *(SET pin control working)*
- [x] Display shows sensor mode correctly *(SENSOR/RELAY modes verified)*
- [x] MQTT messages published in correct format *(JSON to test.mosquitto.org)*
- [x] Serial commands work for testing *(sensors/mqtt commands functional)*
- [x] Build succeeds with no errors *(375KB firmware)*
- [x] Duty cycle tracking includes sensor packets *(recordAirtime called)*

**SENSOR INTEGRATION COMPLETE** - All tasks verified working on 3-node mesh (2026-01-29)

---

## MQTT Configuration Reference

### Broker Settings
| Setting | Default | NVS Key | Serial Command |
|---------|---------|---------|----------------|
| Broker hostname | (empty = disabled) | `mqtt_host` | `mqtt <hostname>` |
| Port | 1883 | `mqtt_port` | - |
| Username | (none) | `mqtt_user` | - |
| Password | (none) | `mqtt_pass` | - |

### Topic Structure
```
xmesh/sensors/{gateway_addr}/{node_addr}

Example:
xmesh/sensors/A1B2/C3D4
```

### Message Schema (JSON)
```json
{
  "version": 1,
  "node": "C3D4",
  "gateway": "A1B2",
  "timestamp": 1706540000000,
  "pm": {
    "pm1_0": 12,
    "pm2_5": 25,
    "pm10": 35,
    "valid": true
  },
  "gps": {
    "lat": 13.7563,
    "lon": 100.5018,
    "alt": 15,
    "sats": 8,
    "valid": true
  }
}
```

### Testing with Public Broker
```bash
# Subscribe to test broker
mosquitto_sub -h test.mosquitto.org -t "xmesh/sensors/#" -v

# Configure gateway via serial
mqtt test.mosquitto.org
wifi on
```
