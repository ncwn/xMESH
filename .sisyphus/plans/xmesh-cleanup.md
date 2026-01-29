# xMESH Production Cleanup Work Plan

## TL;DR

> **Quick Summary**: Professional cleanup of xMESH codebase to integrate unused modules (OTA with rollback safety, Display HAL, real sensor drivers), fix linker errors, standardize logging, implement MeshConfig, and remove dead code.
> 
> **Deliverables**:
> - VersionControl.cpp (fixes linker error)
> - OTAManager integration in main.cpp (enables rollback safety)
> - Real PMS7003 + NEO-M8N sensor drivers
> - Display HAL integration
> - Centralized MeshConfig struct
> - ESP_LOGX standardization in library code
> 
> **Estimated Effort**: Large (12-16 hours)
> **Parallel Execution**: YES - 4 waves
> **Critical Path**: Task 1 → Task 2 → Task 6 → Task 11

---

## Context

### Original Request
Clean up xMESH production codebase after initial refactor. Address 9 identified issues across critical, medium, and low priority. User has real PMS7003 and NEO-M8N hardware for sensor integration.

### Interview Summary
**Key Discussions**:
- OTA: Integrate OTAManager to enable boot-failure rollback safety
- HAL: Full sensor implementation with real hardware (PMS7003 + NEO-M8N GPS)
- MeshConfig: Implement properly as centralized config struct
- Logging: Migrate library code only (~60 calls), keep Serial for CLI
- Scope: Thorough - fix ALL issues
- Build: User can verify locally with `pio run`

**Research Findings**:
- **OTAManager**: Provides NVS-backed boot failure tracking (3 failures = rollback)
- **Sensor Libraries**: PMserial (avaldebe) for PMS7003, TinyGPSPlus for NEO-M8N
- **Both sensors**: 9600 baud UART
- **Header paths**: xmesh-hal uses `include/hal/` should be `include/xmesh/hal/`

### Sensor Hardware Details
| Sensor | Model | Protocol | Baud | Library |
|--------|-------|----------|------|---------|
| PM | PMS7003 | UART Binary | 9600 | avaldebe/PMserial |
| GPS | NEO-M8N | NMEA ASCII | 9600 | mikalhart/TinyGPSPlus |
| Pins | GPIO 4,5,6,7 | TX/RX assignment TBD during hardware test |

---

## Work Objectives

### Core Objective
Transform the xMESH codebase from "refactor complete" to "production-ready" by integrating unused modules, fixing all identified issues, and enabling real sensor hardware support.

### Concrete Deliverables
1. `lib/xmesh-ota/src/VersionControl.cpp` - Fixes linker error
2. `firmware/production/src/main.cpp` - OTA + Display + Sensors integration
3. `lib/xmesh-hal/src/Sensors.cpp` - Real PMS7003 + GPS drivers
4. `lib/xmesh-core/include/xmesh/MeshConfig.h` - Centralized routing config
5. `firmware/production/include/config.h` - Sensor pin definitions
6. Standardized ESP_LOGX in all library files

### Definition of Done
- [ ] `pio run` completes with 0 errors, 0 warnings
- [ ] All CRITICAL/MEDIUM/LOW issues from analysis resolved
- [ ] No Serial.printf in library code (xmesh-core, xmesh-ota)
- [ ] Documentation matches implementation

### Must Have
- Boot-failure rollback safety (OTAManager integrated)
- Real sensor drivers (PMS7003 + GPS)
- Display shows mesh status
- Centralized MeshConfig

### Must NOT Have (Guardrails)
- **DO NOT** modify Trickle algorithm logic (per AGENTS.md)
- **DO NOT** modify Cost Router algorithm logic (per AGENTS.md)
- **DO NOT** modify LoRaMesher library source (upstream dependency)
- **DO NOT** change Serial.printf in main.cpp CLI commands (intentional for interactive use)
- **DO NOT** assume sensor pin assignments are final (verify during hardware test)

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: YES (PlatformIO build system)
- **User wants tests**: Manual verification (no unit test framework)
- **Framework**: `pio run` for compilation, hardware test for sensors

### Automated Verification (Build)
Each task includes:
```bash
pio run  # Must complete with 0 errors
```

### Manual Verification (Deferred)
Sensor functionality requires physical hardware - marked as post-implementation verification.

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately):
├── Task 1: Create VersionControl.cpp [no dependencies]
├── Task 3: Standardize xmesh-hal header paths [no dependencies]
├── Task 7: Fix CostRouter.h documentation [no dependencies]
└── Task 9: Remove unused GatewayBalancer constants [no dependencies]

Wave 2 (After Wave 1):
├── Task 2: Integrate OTAManager into main.cpp [depends: 1]
├── Task 4: Implement MeshConfig struct [depends: 1]
├── Task 5: Add sensor pin config to config.h [depends: 3]
└── Task 10: Standardize logging in xmesh-ota [depends: 1]

Wave 3 (After Wave 2):
├── Task 6: Implement PMS7003 + GPS drivers [depends: 3, 5]
├── Task 8: Document OTA dead code [depends: 2]
└── Task 11: Standardize logging in xmesh-core [depends: 4]

Wave 4 (After Wave 3):
├── Task 12: Integrate Display HAL into main.cpp [depends: 3, 6]
├── Task 13: Integrate Sensors into main.cpp [depends: 6, 12]
└── Task 14: Final build verification [depends: all]

Critical Path: Task 1 → Task 2 → Task 6 → Task 12 → Task 14
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|------------|--------|------|
| 1 | None | 2, 4, 10 | 1 |
| 2 | 1 | 8, 14 | 2 |
| 3 | None | 5, 6, 12 | 1 |
| 4 | 1 | 11, 14 | 2 |
| 5 | 3 | 6 | 2 |
| 6 | 3, 5 | 12, 13 | 3 |
| 7 | None | 14 | 1 |
| 8 | 2 | 14 | 3 |
| 9 | None | 14 | 1 |
| 10 | 1 | 14 | 2 |
| 11 | 4 | 14 | 3 |
| 12 | 3, 6 | 13, 14 | 4 |
| 13 | 6, 12 | 14 | 4 |
| 14 | all | None | 4 |

---

## TODOs

### Wave 1: Foundation (No Dependencies)

---

- [ ] **1. Create VersionControl.cpp** [CRITICAL]

  **What to do**:
  - Create `lib/xmesh-ota/src/VersionControl.cpp`
  - Define static member `Version VersionControl::available_version_`
  - Implement `getCurrentVersion()` using `esp_ota_get_app_description()`
  - Implement `getAvailableVersion()`, `setAvailableVersion()`, `compareVersions()`, `isUpdateAvailable()`

  **Must NOT do**:
  - Add any HTTP/network code (future feature)
  - Modify VersionControl.h

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required - straightforward implementation from header

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 3, 7, 9)
  - **Blocks**: Tasks 2, 4, 10
  - **Blocked By**: None

  **References**:
  - `lib/xmesh-ota/include/ota/VersionControl.h` - Full API specification
  - `lib/xmesh-ota/src/OTAManager.cpp:23` - Example of `esp_ota_get_app_description()` usage pattern

  **Acceptance Criteria**:
  ```bash
  pio run  # Must compile without linker errors for VersionControl
  grep -l "available_version_" lib/xmesh-ota/src/VersionControl.cpp  # Static member defined
  ```

  **Commit**: YES
  - Message: `fix(ota): implement VersionControl.cpp to resolve linker errors`
  - Files: `lib/xmesh-ota/src/VersionControl.cpp`

---

- [ ] **3. Standardize xmesh-hal header paths** [MEDIUM]

  **What to do**:
  - Rename `lib/xmesh-hal/include/hal/` to `lib/xmesh-hal/include/xmesh/hal/`
  - Update `#include` statements in Display.cpp, Sensors.cpp
  - Verify namespace `xmesh::hal` matches path convention

  **Must NOT do**:
  - Change any API signatures
  - Modify implementation logic

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 7, 9)
  - **Blocks**: Tasks 5, 6, 12
  - **Blocked By**: None

  **References**:
  - `lib/xmesh-core/include/xmesh/` - Correct path convention to follow
  - `lib/xmesh-hal/include/hal/Display.h` - Current (incorrect) path
  - `lib/xmesh-hal/include/hal/Sensors.h` - Current (incorrect) path

  **Acceptance Criteria**:
  ```bash
  ls lib/xmesh-hal/include/xmesh/hal/Display.h  # New path exists
  ls lib/xmesh-hal/include/xmesh/hal/Sensors.h  # New path exists
  ! ls lib/xmesh-hal/include/hal/ 2>/dev/null   # Old path removed
  pio run  # Compiles successfully
  ```

  **Commit**: YES
  - Message: `refactor(hal): standardize header paths to include/xmesh/hal/`
  - Files: `lib/xmesh-hal/include/xmesh/hal/*`, `lib/xmesh-hal/src/*.cpp`

---

- [ ] **7. Fix CostRouter.h documentation** [LOW]

  **What to do**:
  - Review line 12 formula documentation in `CostRouter.h`
  - Compare with actual implementation in `CostRouter.cpp`
  - Update documentation to match implementation

  **Must NOT do**:
  - Modify algorithm logic (per AGENTS.md guardrail)
  - Change any code behavior

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3, 9)
  - **Blocks**: Task 14
  - **Blocked By**: None

  **References**:
  - `lib/xmesh-core/include/xmesh/CostRouter.h:12` - Documentation to fix
  - `lib/xmesh-core/src/CostRouter.cpp` - Actual implementation
  - `firmware/production/include/config.h:27-33` - Weight definitions

  **Acceptance Criteria**:
  ```bash
  # Documentation formula matches implementation weights (W1-W5)
  grep -A5 "cost =" lib/xmesh-core/include/xmesh/CostRouter.h  # Shows correct formula
  ```

  **Commit**: YES (groups with Task 9)
  - Message: `docs(core): fix CostRouter formula documentation`
  - Files: `lib/xmesh-core/include/xmesh/CostRouter.h`

---

- [ ] **9. Clean up unused GatewayBalancer constants** [MEDIUM]

  **What to do**:
  - Check if `LOAD_SWITCH_THRESHOLD` and `MAX_GATEWAY_CANDIDATES` in GatewayBalancer.h are redundant with config.h definitions
  - If redundant: remove from GatewayBalancer.h
  - If needed: refactor to use config.h values

  **Must NOT do**:
  - Change GatewayBalancer algorithm behavior

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3, 7)
  - **Blocks**: Task 14
  - **Blocked By**: None

  **References**:
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h` - Constants to review
  - `lib/xmesh-core/src/GatewayBalancer.cpp` - Usage patterns
  - `firmware/production/include/config.h:56-57` - Authoritative definitions

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  # No duplicate constant definitions between GatewayBalancer.h and config.h
  ```

  **Commit**: YES (groups with Task 7)
  - Message: `refactor(core): consolidate GatewayBalancer constants`
  - Files: `lib/xmesh-core/include/xmesh/GatewayBalancer.h`

---

### Wave 2: OTA & Config (After Wave 1)

---

- [ ] **2. Integrate OTAManager into main.cpp** [CRITICAL]

  **What to do**:
  - Replace direct ArduinoOTA usage with `xmesh::ota::OTAManager`
  - Add `#include <xmesh/ota/OTAManager.h>`
  - Instantiate `OTAManager otaManager` global
  - In `initOTA()`: call `otaManager.begin()` instead of ArduinoOTA setup
  - In `loop()`: call `otaManager.process()` instead of `ArduinoOTA.handle()`
  - **CRITICAL**: Call `otaManager.markAppValid()` at END of `setup()` after all init succeeds
  - Remove redundant ArduinoOTA callback setup

  **Must NOT do**:
  - Remove WiFi connection logic (still needed)
  - Remove hostname/password configuration (pass to OTAManager if supported)

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering` (main.cpp is core firmware)
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 4, 5, 10)
  - **Blocks**: Tasks 8, 14
  - **Blocked By**: Task 1

  **References**:
  - `lib/xmesh-ota/include/ota/OTAManager.h` - Full API
  - `lib/xmesh-ota/src/OTAManager.cpp` - Implementation details
  - `firmware/production/src/main.cpp` - Current ArduinoOTA code to replace

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  grep "otaManager.markAppValid" firmware/production/src/main.cpp  # Safety call present
  grep "otaManager.process" firmware/production/src/main.cpp  # Loop handler present
  ! grep "ArduinoOTA.handle" firmware/production/src/main.cpp  # Old code removed
  ```

  **Commit**: YES
  - Message: `feat(ota): integrate OTAManager with boot-failure rollback safety`
  - Files: `firmware/production/src/main.cpp`

---

- [ ] **4. Implement MeshConfig struct** [MEDIUM]

  **What to do**:
  - Populate `lib/xmesh-core/include/xmesh/MeshConfig.h` with routing parameters
  - Include: Trickle params (I_MIN, I_MAX, K), Cost weights (W1-W5), ETX params, normalization ranges
  - Add static factory method `MeshConfig::defaultConfig()` returning production defaults
  - Keep firmware config.h for hardware-specific settings only

  **Must NOT do**:
  - Move hardware config (pins, LoRa frequency) to MeshConfig
  - Change xmesh-core module constructors yet (defer to future refactor)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 5, 10)
  - **Blocks**: Task 11, 14
  - **Blocked By**: Task 1

  **References**:
  - `lib/xmesh-core/include/xmesh/MeshConfig.h` - Empty placeholder to populate
  - `firmware/production/include/config.h:14-61` - Parameters to centralize
  - `.sisyphus/plans/xmesh-production-refactor.md` - Original design intent

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  grep "struct MeshConfig" lib/xmesh-core/include/xmesh/MeshConfig.h  # Struct exists
  grep "TRICKLE_I_MIN" lib/xmesh-core/include/xmesh/MeshConfig.h  # Trickle params present
  grep "defaultConfig" lib/xmesh-core/include/xmesh/MeshConfig.h  # Factory method exists
  ```

  **Commit**: YES
  - Message: `feat(core): implement MeshConfig centralized routing configuration`
  - Files: `lib/xmesh-core/include/xmesh/MeshConfig.h`

---

- [ ] **5. Add sensor pin configuration to config.h** [MEDIUM]

  **What to do**:
  - Add new section "Sensor Configuration" to `firmware/production/include/config.h`
  - Define GPIO pins: `SENSOR_PMS_RX`, `SENSOR_PMS_TX`, `SENSOR_GPS_RX`, `SENSOR_GPS_TX`
  - Use GPIO 4, 5, 6, 7 as placeholders (user will verify during hardware test)
  - Add baud rate constants: `SENSOR_PMS_BAUD = 9600`, `SENSOR_GPS_BAUD = 9600`
  - Add enable flags: `SENSORS_ENABLED = true`

  **Must NOT do**:
  - Finalize pin assignments (marked as TBD for hardware test)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 4, 10)
  - **Blocks**: Task 6
  - **Blocked By**: Task 3

  **References**:
  - `firmware/production/include/config.h` - Target file
  - `firmware/production/include/config.h:74-79` - OLED section pattern to follow

  **Acceptance Criteria**:
  ```bash
  grep "SENSOR_PMS_RX" firmware/production/include/config.h  # PM sensor pins defined
  grep "SENSOR_GPS_RX" firmware/production/include/config.h  # GPS pins defined
  grep "SENSORS_ENABLED" firmware/production/include/config.h  # Enable flag present
  ```

  **Commit**: YES (groups with Task 6)
  - Message: `feat(config): add sensor pin configuration for PMS7003 and GPS`
  - Files: `firmware/production/include/config.h`

---

- [ ] **10. Standardize logging in xmesh-ota** [MEDIUM]

  **What to do**:
  - Replace all `Serial.printf` in `OTAManager.cpp` with `ESP_LOGX` macros (~29 occurrences)
  - Add `#include <esp_log.h>` if missing
  - Define `static const char* TAG = "OTA";`
  - Map contexts: errors → ESP_LOGE, warnings → ESP_LOGW, progress → ESP_LOGI, debug → ESP_LOGD

  **Must NOT do**:
  - Change log message content (only the logging mechanism)
  - Remove any logging (preserve all existing log points)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required - mechanical replacement

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 4, 5)
  - **Blocks**: Task 14
  - **Blocked By**: Task 1

  **References**:
  - `lib/xmesh-ota/src/OTAManager.cpp` - Target file (~29 Serial.printf)
  - `lib/xmesh-core/src/CostRouter.cpp` - Pattern to follow (has proper ESP_LOG usage)
  - `AGENTS.md` - Logging requirement specification

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  ! grep "Serial.printf" lib/xmesh-ota/src/OTAManager.cpp  # No Serial.printf remaining
  grep 'TAG = "OTA"' lib/xmesh-ota/src/OTAManager.cpp  # LOG_TAG defined
  ```

  **Commit**: YES (groups with Task 11)
  - Message: `refactor(ota): standardize logging to ESP_LOGX macros`
  - Files: `lib/xmesh-ota/src/OTAManager.cpp`

---

### Wave 3: Sensors & Logging (After Wave 2)

---

- [ ] **6. Implement PMS7003 + GPS sensor drivers** [CRITICAL]

  **What to do**:
  - Add library dependencies to `firmware/production/platformio.ini`:
    - `avaldebe/PMserial @ ^1.1.0`
    - `mikalhart/TinyGPSPlus @ ^1.0.3`
  - Rewrite `lib/xmesh-hal/src/Sensors.cpp` with real implementations:
    - `beginAirQuality()`: Initialize SerialPM with configured pins/baud
    - `beginGPS()`: Initialize HardwareSerial + TinyGPSPlus with configured pins/baud
    - `readAirQuality()`: Read PM1.0/PM2.5/PM10 from SerialPM, populate AirQualityData
    - `readGPS()`: Parse NMEA via TinyGPSPlus, populate GPSData
    - `update()`: Non-blocking GPS feed loop (call `gps.encode()`)
  - Store library instances as opaque pointers (existing pattern)

  **Must NOT do**:
  - Hardcode GPIO pins (use config.h values)
  - Block in update() (must be non-blocking for mesh loop)

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
  - **Skills**: Load `frontend-ui-ux` for clean API design (optional)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 8, 11)
  - **Blocks**: Tasks 12, 13
  - **Blocked By**: Tasks 3, 5

  **References**:
  - `lib/xmesh-hal/include/xmesh/hal/Sensors.h` - API to implement
  - `lib/xmesh-hal/src/Sensors.cpp` - Stub to replace
  - PMserial docs: https://github.com/avaldebe/PMserial
  - TinyGPSPlus docs: https://github.com/mikalhart/TinyGPSPlus
  - `firmware/production/include/config.h` - Pin definitions (after Task 5)

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  grep "SerialPM" lib/xmesh-hal/src/Sensors.cpp  # PMserial library used
  grep "TinyGPSPlus" lib/xmesh-hal/src/Sensors.cpp  # TinyGPSPlus library used
  ! grep "return false" lib/xmesh-hal/src/Sensors.cpp  # No stub returns
  ```

  **Commit**: YES
  - Message: `feat(hal): implement PMS7003 and NEO-M8N GPS sensor drivers`
  - Files: `lib/xmesh-hal/src/Sensors.cpp`, `firmware/production/platformio.ini`

---

- [ ] **8. Document OTA dead code** [LOW]

  **What to do**:
  - Add TODO comments to `checkForUpdates()` explaining it's a stub for future HTTP OTA
  - Add TODO comments to `startUpdate(url)` explaining HTTP path is not implemented
  - Add TODO comment to `verifyPartition()` explaining it's internal and called by begin()
  - Do NOT remove these methods (they're part of the API surface for future features)

  **Must NOT do**:
  - Remove dead code (preserve for future HTTP OTA feature)
  - Change function signatures

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 6, 11)
  - **Blocks**: Task 14
  - **Blocked By**: Task 2

  **References**:
  - `lib/xmesh-ota/src/OTAManager.cpp` - Methods to document
  - `lib/xmesh-ota/include/ota/OTAManager.h` - API declarations

  **Acceptance Criteria**:
  ```bash
  grep "TODO.*HTTP OTA" lib/xmesh-ota/src/OTAManager.cpp  # Future work documented
  grep "TODO" lib/xmesh-ota/include/ota/OTAManager.h  # API docs note stub status
  ```

  **Commit**: YES (groups with Task 10)
  - Message: `docs(ota): document stub methods for future HTTP OTA support`
  - Files: `lib/xmesh-ota/src/OTAManager.cpp`, `lib/xmesh-ota/include/ota/OTAManager.h`

---

- [ ] **11. Standardize logging in xmesh-core** [MEDIUM]

  **What to do**:
  - Replace all `Serial.printf` in xmesh-core library files:
    - `ETXTracker.cpp` (~5 occurrences)
    - `GatewayBalancer.cpp` (~18 occurrences)
    - `TrickleScheduler.cpp` (~6 occurrences)
  - Each file: add `#include <esp_log.h>`, define `static const char* TAG`
  - Use existing TAGs from ESP_LOG calls where present

  **Must NOT do**:
  - Change log message content
  - Modify CostRouter.cpp (already compliant)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required - mechanical replacement

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 6, 8)
  - **Blocks**: Task 14
  - **Blocked By**: Task 4

  **References**:
  - `lib/xmesh-core/src/ETXTracker.cpp` - 5 Serial.printf to replace
  - `lib/xmesh-core/src/GatewayBalancer.cpp` - 18 Serial.printf to replace
  - `lib/xmesh-core/src/TrickleScheduler.cpp` - 6 Serial.printf to replace
  - `lib/xmesh-core/src/CostRouter.cpp` - Reference pattern (already correct)

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  ! grep "Serial.printf" lib/xmesh-core/src/ETXTracker.cpp
  ! grep "Serial.printf" lib/xmesh-core/src/GatewayBalancer.cpp
  ! grep "Serial.printf" lib/xmesh-core/src/TrickleScheduler.cpp
  ```

  **Commit**: YES (groups with Task 10)
  - Message: `refactor(core): standardize logging to ESP_LOGX macros`
  - Files: `lib/xmesh-core/src/*.cpp`

---

### Wave 4: Integration & Verification (After Wave 3)

---

- [ ] **12. Integrate Display HAL into main.cpp** [CRITICAL]

  **What to do**:
  - Add `#include <xmesh/hal/Display.h>` to main.cpp
  - Instantiate `xmesh::hal::Display display` global
  - Call `display.begin()` in `setup()` after Wire.begin()
  - Add `updateDisplay()` helper function showing:
    - Node address
    - Neighbor count
    - Gateway status (YES/NO)
    - Last sensor reading (PM2.5 or GPS fix status)
  - Call `updateDisplay()` periodically in loop (every 5s)

  **Must NOT do**:
  - Create complex graphics (keep it simple per user request)
  - Remove Serial logging (display is supplementary)

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
  - **Skills**: `frontend-ui-ux` for clean display layout

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with Tasks 13)
  - **Blocks**: Task 13, 14
  - **Blocked By**: Tasks 3, 6

  **References**:
  - `lib/xmesh-hal/include/xmesh/hal/Display.h` - API to use
  - `lib/xmesh-hal/src/Display.cpp` - Implementation
  - `firmware/production/include/config.h:74-79` - OLED pin definitions

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  grep "display.begin" firmware/production/src/main.cpp  # Display initialized
  grep "updateDisplay" firmware/production/src/main.cpp  # Update function exists
  ```

  **Commit**: YES
  - Message: `feat(hal): integrate Display HAL for OLED status screen`
  - Files: `firmware/production/src/main.cpp`

---

- [ ] **13. Integrate Sensors into main.cpp** [CRITICAL]

  **What to do**:
  - Add `#include <xmesh/hal/Sensors.h>` to main.cpp
  - Instantiate `xmesh::hal::Sensors sensors` global
  - In `setup()`:
    - Initialize Serial1 for PMS at configured pins/baud
    - Initialize Serial2 for GPS at configured pins/baud
    - Call `sensors.beginAirQuality(&Serial1)`
    - Call `sensors.beginGPS(&Serial2)`
  - In `loop()`:
    - Call `sensors.update()` every iteration (non-blocking GPS feed)
    - Every 10s: read sensor data, log to Serial, include in mesh packets
  - Update `updateDisplay()` to show sensor readings

  **Must NOT do**:
  - Finalize packet format for sensor data (document as TODO for protocol design)
  - Block the mesh loop

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with Task 12)
  - **Blocks**: Task 14
  - **Blocked By**: Tasks 6, 12

  **References**:
  - `lib/xmesh-hal/include/xmesh/hal/Sensors.h` - API to use
  - `lib/xmesh-hal/src/Sensors.cpp` - Implementation (after Task 6)
  - `firmware/production/include/config.h` - Sensor pin config (after Task 5)
  - `firmware/production/src/main.cpp` - Integration target

  **Acceptance Criteria**:
  ```bash
  pio run  # Compiles successfully
  grep "sensors.beginAirQuality" firmware/production/src/main.cpp  # PM sensor init
  grep "sensors.beginGPS" firmware/production/src/main.cpp  # GPS init
  grep "sensors.update" firmware/production/src/main.cpp  # Non-blocking update in loop
  ```

  **Commit**: YES
  - Message: `feat(hal): integrate PMS7003 and GPS sensors into production firmware`
  - Files: `firmware/production/src/main.cpp`

---

- [ ] **14. Final build verification** [CRITICAL]

  **What to do**:
  - Run full clean build: `pio run -t clean && pio run`
  - Verify 0 errors, 0 warnings
  - Verify no linker errors for VersionControl
  - Verify all includes resolve correctly
  - Check binary size is reasonable (<1.9MB for OTA partition)
  - Document any remaining TODOs or hardware-test-required items

  **Must NOT do**:
  - Skip this verification
  - Proceed if any errors exist

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: NO (final task)
  - **Parallel Group**: None
  - **Blocks**: None (completion)
  - **Blocked By**: ALL previous tasks

  **References**:
  - All modified files from Tasks 1-13
  - `firmware/production/platformio.ini` - Build configuration
  - `firmware/production/partitions.csv` - Partition size limits

  **Acceptance Criteria**:
  ```bash
  pio run -t clean && pio run
  # Output: SUCCESS with 0 errors, 0 warnings
  # Binary size < 1900000 bytes (OTA partition limit)
  ```

  **Commit**: NO (verification only)

---

## Commit Strategy

| After Task | Message | Files |
|------------|---------|-------|
| 1 | `fix(ota): implement VersionControl.cpp` | lib/xmesh-ota/src/VersionControl.cpp |
| 2 | `feat(ota): integrate OTAManager with rollback safety` | firmware/production/src/main.cpp |
| 3 | `refactor(hal): standardize header paths` | lib/xmesh-hal/** |
| 4 | `feat(core): implement MeshConfig` | lib/xmesh-core/include/xmesh/MeshConfig.h |
| 5+6 | `feat(hal): implement sensor drivers with config` | Sensors.cpp, config.h, platformio.ini |
| 7+9 | `refactor(core): fix docs and clean up constants` | CostRouter.h, GatewayBalancer.h |
| 8+10+11 | `refactor: standardize logging and document stubs` | xmesh-ota/*, xmesh-core/* |
| 12+13 | `feat(hal): integrate Display and Sensors` | firmware/production/src/main.cpp |

---

## Success Criteria

### Verification Commands
```bash
# Clean build with no errors
pio run -t clean && pio run
# Expected: SUCCESS, 0 errors, 0 warnings

# No Serial.printf in libraries
grep -r "Serial.printf" lib/
# Expected: No matches (or only in main.cpp CLI)

# All new files exist
ls lib/xmesh-ota/src/VersionControl.cpp
ls lib/xmesh-hal/include/xmesh/hal/Display.h
# Expected: Files exist

# OTA safety integrated
grep "markAppValid" firmware/production/src/main.cpp
# Expected: Match found
```

### Final Checklist
- [ ] All CRITICAL issues resolved (VersionControl, OTA integration, HAL integration)
- [ ] All MEDIUM issues resolved (MeshConfig, constants, logging)
- [ ] All LOW issues resolved (docs, dead code documented, sensors implemented)
- [ ] No Serial.printf in library code
- [ ] Build succeeds with 0 errors, 0 warnings
- [ ] Binary size < 1.9MB

---

## Post-Implementation Notes

### Hardware Testing Required (Deferred)
After flashing to real Heltec V3 devices:
1. Verify sensor pin assignments (GPIO 4,5,6,7 TX/RX order)
2. Test OTA rollback by uploading crashing firmware
3. Verify display shows mesh status correctly
4. Test GPS fix acquisition and PM sensor readings
5. Verify sensor data appears in mesh packets

### Future Work Identified
- HTTP OTA support (currently stub)
- High-level display widgets (routing table, signal strength bars)
- Sensor data protocol design for mesh transmission
- MeshConfig integration into xmesh-core constructors

---

This plan addresses all 9 identified issues and transforms xMESH from "refactor complete" to "production-ready" with real sensor support.
