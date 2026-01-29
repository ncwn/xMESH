# xMESH Production Features: Serial CLI + WiFi OTA + Stability Test

## TL;DR

> **Quick Summary**: Add runtime serial commands with NVS persistence, enable WiFi-based ArduinoOTA on gateway nodes only, and validate mesh stability over a 4-hour test run.
> 
> **Deliverables**:
> - `main.cpp` with serial command handler + NVS integration
> - Working OTA updates on gateway node via WiFi
> - 4-hour stability test log with evidence captured
> - Atomic commits for each feature
> 
> **Estimated Effort**: Medium (6-8 hours)
> **Parallel Execution**: YES - 2 waves
> **Critical Path**: Task 1 (NVS) -> Task 2 (Serial) -> Task 3 (OTA) -> Task 4 (Stability)

---

## Context

### Original Request
Add three production features to xMESH LoRa mesh firmware:
1. Serial commands (`gateway on/off`, `wifi SSID PASSWORD`, `status`, `reset trickle`) with NVS persistence
2. WiFi OTA for gateway node only using ArduinoOTA
3. 4-hour stability test with heap/watchdog/routing monitoring

### Hardware Configuration
- **Board**: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 + SSD1306)
- **Nodes**: 3 units at USB ports `/dev/cu.usbserial-0001`, `-4`, `-5`
- **Flash**: 4MB with OTA partition scheme

### Research Findings

**Firmware State** (from explore agent):
- `main.cpp` has setup/loop structure, watchdog, mesh initialization
- NO serial command parsing exists (Serial used only for logging)
- Gateway role is compile-time constant `IS_GATEWAY_NODE` in config.h
- No NVS usage in application code (only OTA boot tracking)

**OTA Module State**:
- `OTAManager` is 80% complete (callbacks, partition handling, rollback)
- **Missing**: WiFi connection logic
- **Missing**: Integration in main.cpp loop
- ArduinoOTA is built-in with Arduino framework

**Core Interfaces Available**:
- `trickle.reset()` - Force Trickle timer to I_min
- `gatewayBalancer.setIsGateway(bool)` - Runtime gateway role change
- `gatewayBalancer.getIsGateway()` - Check current role
- `gatewayBalancer.getNeighborCount()` - Get neighbor count
- `radio.routingTableSize()` - Get routing table size
- `trickle.getTransmitCount()` / `getSuppressCount()` - Trickle stats

---

## Work Objectives

### Core Objective
Enable runtime configuration of xMESH nodes via serial commands with persistent storage, add remote firmware update capability for gateway nodes, and validate system stability under sustained operation.

### Concrete Deliverables
1. `firmware/production/src/main.cpp` - Updated with serial handler + NVS + OTA
2. `firmware/production/include/config.h` - WiFi and OTA configuration constants
3. `.sisyphus/evidence/stability-test.log` - 4-hour test output
4. `.sisyphus/evidence/heap-plot.png` - Heap usage visualization (optional)

### Definition of Done
- [ ] `gateway on` command persists to NVS and survives reboot
- [ ] `wifi SSID PASSWORD` stores credentials and gateway connects
- [ ] `status` prints node address, neighbor count, routing table size
- [ ] `reset trickle` forces immediate HELLO transmission
- [ ] OTA upload via `pio run -t upload --upload-port <IP>` succeeds on gateway
- [ ] 4-hour stability test passes (heap > 50KB, no WDT resets, stable routes)

### Must Have
- NVS persistence for `is_gateway`, `wifi_ssid`, `wifi_pass`
- Gateway-only WiFi connection (non-gateway nodes stay WiFi-off)
- Non-blocking OTA (mesh continues during idle OTA checks)
- ESP-IDF logging (ESP_LOGI, ESP_LOGE, etc.)
- xmesh namespace for all library code

### Must NOT Have (Guardrails)
- **NO WiFi on non-gateway nodes** - Only gateway connects to WiFi
- **NO blocking serial reads** - Use `Serial.available()` check pattern
- **NO changes to routing algorithms** - CostRouter, Trickle math untouched
- **NO HTTP OTA** - Only ArduinoOTA (LAN-based) for this phase
- **NO plaintext password in code** - Use config.h constant or NVS
- **NO modification to xmesh-core/** - All changes in firmware/production/

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: NO (no unit tests for firmware)
- **User wants tests**: Manual verification + stability test
- **Framework**: N/A (hardware testing)
- **QA approach**: Automated serial/terminal verification + 4-hour stability test

### Automated Verification Approach

All acceptance criteria are verified via:
1. **Serial commands**: Send command via `pio device monitor`, check output
2. **NVS persistence**: Reboot node, verify settings retained
3. **OTA**: Upload via espota, verify success
4. **Stability**: Run 4 hours, grep logs for errors/warnings

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately):
├── Task 1: NVS Configuration Manager
└── Task 5: Write stability test procedure document

Wave 2 (After Task 1):
├── Task 2: Serial Command Handler (depends: 1)
├── Task 3: WiFi + OTA Integration (depends: 1)
└── Task 4: Gateway-Only WiFi Logic (depends: 1)

Wave 3 (After Wave 2):
└── Task 6: Run 4-hour Stability Test (depends: 2, 3, 4)

Critical Path: Task 1 → Task 2 → Task 6
Parallel Speedup: ~35% faster than sequential
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 1 | None | 2, 3, 4 | 5 |
| 2 | 1 | 6 | 3, 4 |
| 3 | 1 | 6 | 2, 4 |
| 4 | 1 | 6 | 2, 3 |
| 5 | None | 6 | 1 |
| 6 | 2, 3, 4, 5 | None | None (final) |

### Agent Dispatch Summary

| Wave | Tasks | Recommended Dispatch |
|------|-------|---------------------|
| 1 | 1, 5 | Run in parallel - independent foundation work |
| 2 | 2, 3, 4 | Run in parallel after Task 1 completes |
| 3 | 6 | Sequential final verification |

---

## TODOs

### Task 1: NVS Configuration Manager

- [ ] 1. Create NVS configuration helper in main.cpp

  **What to do**:
  - Add NVS includes (`nvs_flash.h`, `nvs.h`)
  - Create `initNVS()` function with proper error handling (erase on corruption)
  - Create `ConfigManager` struct or functions for:
    - `loadConfig()` - Load is_gateway, wifi_ssid, wifi_pass from NVS with defaults
    - `saveGatewayRole(bool)` - Persist gateway flag
    - `saveWiFiCredentials(ssid, password)` - Persist WiFi creds
  - Open NVS namespace "xmesh_cfg" in setup()
  - Load config on boot and apply to gatewayBalancer

  **Must NOT do**:
  - Create separate .cpp/.h files (keep in main.cpp for simplicity)
  - Use Preferences.h Arduino library (use native ESP-IDF NVS)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Focused single-file modification with clear API patterns
  - **Skills**: None required
    - ESP-IDF NVS is well-documented, patterns provided in research

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 5)
  - **Blocks**: Tasks 2, 3, 4
  - **Blocked By**: None

  **References**:
  
  **Pattern References**:
  - Research: NVS init pattern with `ESP_ERR_NVS_NO_FREE_PAGES` handling
  - Research: `nvs_set_u8` for bool, `nvs_set_str` for strings
  - Research: Always call `nvs_commit()` after writes
  
  **API/Type References**:
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h` - `setIsGateway(bool)` signature
  - `firmware/production/src/main.cpp:45-50` - Current setup() initialization order
  
  **External References**:
  - ESP-IDF NVS docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html

  **Acceptance Criteria**:

  ```bash
  # Agent builds and flashes firmware:
  cd /Volumes/xMESH/xMESH/firmware/production && pio run -t upload --upload-port /dev/cu.usbserial-0001
  # Assert: Exit code 0, "SUCCESS" in output
  
  # Agent monitors boot log:
  timeout 10 pio device monitor -p /dev/cu.usbserial-0001 -b 115200 | head -50
  # Assert: Contains "NVS initialized" or "xmesh_cfg namespace opened"
  # Assert: Contains "Config loaded: gateway="
  ```

  **Commit**: YES
  - Message: `feat(firmware): add NVS configuration persistence`
  - Files: `firmware/production/src/main.cpp`
  - Pre-commit: `pio run` (build check)

---

### Task 2: Serial Command Handler

- [ ] 2. Implement serial command parsing in main loop

  **What to do**:
  - Add `processSerialCommands()` function called from loop()
  - Use `Serial.available()` + `Serial.readStringUntil('\n')` for non-blocking reads
  - Parse commands with simple string matching:
    - `gateway on` -> `saveGatewayRole(true)` + `gatewayBalancer.setIsGateway(true)`
    - `gateway off` -> `saveGatewayRole(false)` + `gatewayBalancer.setIsGateway(false)`
    - `wifi SSID PASSWORD` -> Parse args, `saveWiFiCredentials()`, reconnect if gateway
    - `status` -> Print node info (address, neighbor count, route count, heap, gateway status)
    - `reset trickle` -> `trickle.reset()` + confirm message
  - Use ESP_LOGI for command responses (visible in monitor)

  **Must NOT do**:
  - Use `delay()` in command handling
  - Block waiting for complete command
  - Parse complex arguments (keep it simple: space-separated)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Straightforward string parsing with existing examples
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 3, 4)
  - **Blocks**: Task 6
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - `firmware/production/src/main.cpp:loop()` - Current loop structure
  - Research: Non-blocking Serial pattern with `Serial.available()`
  
  **API/Type References**:
  - `lib/xmesh-core/include/xmesh/TrickleScheduler.h` - `reset()` method
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h` - `setIsGateway()`, `getNeighborCount()`
  - LoRaMesher: `radio.routingTableSize()` from research
  - ESP-IDF: `esp_get_free_heap_size()` for heap reporting

  **Acceptance Criteria**:

  ```bash
  # Agent opens serial monitor and sends commands:
  # Test 1: gateway command
  echo "gateway on" | timeout 5 pio device monitor -p /dev/cu.usbserial-0001 -b 115200
  # Assert: Output contains "Gateway mode: ON" or similar confirmation
  
  # Test 2: status command
  echo "status" | timeout 5 pio device monitor -p /dev/cu.usbserial-0001 -b 115200
  # Assert: Output contains "Neighbors:", "Routes:", "Heap:"
  
  # Test 3: reset trickle
  echo "reset trickle" | timeout 5 pio device monitor -p /dev/cu.usbserial-0001 -b 115200
  # Assert: Output contains "Trickle reset" or "Timer reset to I_min"
  ```

  **Commit**: YES
  - Message: `feat(firmware): add serial command interface`
  - Files: `firmware/production/src/main.cpp`
  - Pre-commit: `pio run`

---

### Task 3: WiFi Connection Logic

- [ ] 3. Add WiFi connection for gateway nodes

  **What to do**:
  - Add WiFi includes (`WiFi.h`)
  - Create `initWiFi()` function:
    - Check `gatewayBalancer.getIsGateway()` - skip WiFi if false
    - Load SSID/password from NVS (with fallback to config.h constants)
    - `WiFi.mode(WIFI_STA)` + `WiFi.begin(ssid, pass)`
    - Timeout-based connection (15 seconds max, don't block forever)
    - Log connection result with IP address
  - Call `initWiFi()` in setup() AFTER NVS and gateway role are loaded
  - Handle reconnection in loop() if WiFi drops (optional, low priority)

  **Must NOT do**:
  - Enable WiFi on non-gateway nodes
  - Block setup() indefinitely waiting for WiFi
  - Use WiFi.waitForConnectResult() without timeout

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard WiFi pattern, well-documented
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 4)
  - **Blocks**: Task 6
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - Research: WiFi connection with timeout pattern (15s)
  - Research: `WiFi.mode(WIFI_STA)` for station mode
  
  **API/Type References**:
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h` - `getIsGateway()` check

  **Acceptance Criteria**:

  ```bash
  # Agent flashes gateway-enabled node:
  # First set WiFi credentials via serial: wifi TestSSID TestPassword
  # Then check boot log:
  timeout 20 pio device monitor -p /dev/cu.usbserial-0001 -b 115200 | grep -E "(WiFi|IP|connected)"
  # Assert: Contains "WiFi connected" and shows IP address (192.168.x.x)
  # OR: Contains "WiFi connection failed" if no network available (acceptable for indoor test)
  ```

  **Commit**: Groups with Task 4
  - Message: `feat(firmware): add WiFi OTA for gateway nodes`
  - Files: `firmware/production/src/main.cpp`, `firmware/production/include/config.h`
  - Pre-commit: `pio run`

---

### Task 4: ArduinoOTA Integration

- [ ] 4. Wire OTAManager into main.cpp

  **What to do**:
  - Include OTAManager header: `#include "ota/OTAManager.h"`
  - Add OTA configuration to config.h:
    - `OTA_HOSTNAME_PREFIX` = "xmesh-"
    - `OTA_PASSWORD` = configurable string
  - Create global `xmesh::ota::OTAManager otaManager;`
  - In setup() after WiFi connected:
    - Set hostname: `ArduinoOTA.setHostname(hostname)` based on MAC
    - Set password: `ArduinoOTA.setPassword(OTA_PASSWORD)`
    - Call `otaManager.begin()` only if WiFi connected
  - In loop():
    - Call `otaManager.process()` (non-blocking ArduinoOTA.handle())
    - Skip heavy mesh processing if OTA is actively downloading
  - After stable boot period (10s): call `otaManager.markAppValid()`

  **Must NOT do**:
  - Initialize OTA on non-gateway nodes
  - Initialize OTA if WiFi connection failed
  - Block loop() during OTA idle checks

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Wiring existing OTAManager, minimal new code
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 3)
  - **Blocks**: Task 6
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - `lib/xmesh-ota/src/OTAManager.cpp` - Existing begin()/process() implementation
  - Research: ArduinoOTA.setHostname(), setPassword() patterns
  - Research: Non-blocking process() pattern
  
  **API/Type References**:
  - `lib/xmesh-ota/include/ota/OTAManager.h` - OTAManager class interface
  
  **External References**:
  - ArduinoOTA examples: https://github.com/espressif/arduino-esp32/tree/master/libraries/ArduinoOTA/examples

  **Acceptance Criteria**:

  ```bash
  # Agent verifies OTA is ready:
  timeout 30 pio device monitor -p /dev/cu.usbserial-0001 -b 115200 | grep -E "(OTA|ArduinoOTA)"
  # Assert: Contains "OTA service started" or "ArduinoOTA ready"
  # Assert: Contains hostname like "xmesh-xxxxxxxxxxxx"
  
  # Agent tests OTA upload (requires WiFi network):
  # Get IP from serial log, then:
  # pio run -t upload --upload-port 192.168.1.X
  # Assert: Upload succeeds OR graceful failure if no network
  ```

  **Commit**: YES (combined with Task 3)
  - Message: `feat(firmware): add WiFi OTA for gateway nodes`
  - Files: `firmware/production/src/main.cpp`, `firmware/production/include/config.h`
  - Pre-commit: `pio run`

---

### Task 5: Stability Test Procedure Document

- [ ] 5. Create stability test procedure and pass criteria

  **What to do**:
  - Create `.sisyphus/evidence/stability-test-procedure.md` with:
    - Hardware setup (3 nodes, USB ports, placement)
    - Commands to start monitoring on each node
    - Metrics to capture every 15 minutes:
      - Free heap (via `status` command or boot log)
      - Neighbor count
      - Routing table size
      - Trickle transmit/suppress counts
    - Pass criteria:
      - Heap > 50KB throughout (no significant leak)
      - No watchdog resets (grep for "wdt" or "rst:0x")
      - Route count stable (not growing unbounded)
      - All 3 nodes visible to each other
    - Evidence capture commands for `.sisyphus/evidence/`

  **Must NOT do**:
  - Actually run the test (that's Task 6)
  - Create overly complex monitoring scripts

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation task, no code
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 1)
  - **Blocks**: Task 6
  - **Blocked By**: None

  **References**:

  **Documentation References**:
  - User requirement: 4 hours, indoors, all nodes visible
  - Research: `esp_get_free_heap_size()` for heap monitoring
  - Research: `gatewayBalancer.monitorNeighborHealth()` prints table every 5 min

  **Acceptance Criteria**:

  ```bash
  # Agent verifies document exists:
  cat /Volumes/xMESH/xMESH/.sisyphus/evidence/stability-test-procedure.md
  # Assert: File exists
  # Assert: Contains "Pass Criteria" section
  # Assert: Contains "4 hour" mention
  # Assert: Contains heap threshold (50KB or similar)
  ```

  **Commit**: YES
  - Message: `test(stability): add 4-hour stability test procedure`
  - Files: `.sisyphus/evidence/stability-test-procedure.md`
  - Pre-commit: None

---

### Task 6: Run 4-Hour Stability Test

- [ ] 6. Execute stability test and capture evidence

  **What to do**:
  - Flash all 3 nodes with final firmware
  - Set one node as gateway (via `gateway on` command)
  - Start serial logging on all nodes to separate files:
    - `pio device monitor -p /dev/cu.usbserial-0001 > node1.log &`
    - Similar for nodes 2 and 3
  - Run for 4 hours (or verify with shorter smoke test first)
  - Every hour (or at end):
    - Run `status` command on each node
    - Capture heap, neighbor count, route count
  - After 4 hours:
    - Stop logging
    - Analyze logs for pass criteria
    - Save summary to `.sisyphus/evidence/stability-test.log`
    - Document any issues found

  **Must NOT do**:
  - Abort test if minor issues (document and continue)
  - Modify firmware during test (invalidates results)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: Hardware test requiring terminal monitoring
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3 (final)
  - **Blocks**: None (final task)
  - **Blocked By**: Tasks 2, 3, 4, 5

  **References**:

  **Procedure Reference**:
  - `.sisyphus/evidence/stability-test-procedure.md` - Follow this procedure

  **Acceptance Criteria**:

  ```bash
  # Agent verifies evidence captured:
  ls -la /Volumes/xMESH/xMESH/.sisyphus/evidence/
  # Assert: stability-test.log exists
  # Assert: File size > 1KB (has content)
  
  cat /Volumes/xMESH/xMESH/.sisyphus/evidence/stability-test.log
  # Assert: Contains "PASS" or "FAIL" verdict
  # Assert: Contains heap readings
  # Assert: Contains 4-hour duration confirmation
  ```

  **Commit**: YES
  - Message: `test(stability): complete 4-hour stability test`
  - Files: `.sisyphus/evidence/stability-test.log`
  - Pre-commit: None

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `feat(firmware): add NVS configuration persistence` | main.cpp | `pio run` |
| 2 | `feat(firmware): add serial command interface` | main.cpp | `pio run` |
| 3+4 | `feat(firmware): add WiFi OTA for gateway nodes` | main.cpp, config.h | `pio run` |
| 5 | `test(stability): add 4-hour stability test procedure` | procedure.md | N/A |
| 6 | `test(stability): complete 4-hour stability test` | test.log | N/A |

---

## Success Criteria

### Verification Commands

```bash
# Build verification
cd /Volumes/xMESH/xMESH/firmware/production && pio run
# Expected: SUCCESS, no errors

# Flash all nodes
pio run -t upload --upload-port /dev/cu.usbserial-0001
pio run -t upload --upload-port /dev/cu.usbserial-4
pio run -t upload --upload-port /dev/cu.usbserial-5
# Expected: All succeed

# Serial command test (on any node)
echo "status" | timeout 5 pio device monitor -p /dev/cu.usbserial-0001 -b 115200
# Expected: Shows heap, neighbors, routes

# OTA test (gateway node with WiFi)
pio run -t upload --upload-port 192.168.X.X
# Expected: Upload succeeds over WiFi
```

### Final Checklist
- [ ] All "Must Have" present:
  - [ ] NVS persistence works (survives reboot)
  - [ ] Serial commands functional (all 4 commands)
  - [ ] Gateway WiFi connects
  - [ ] ArduinoOTA accepts uploads
- [ ] All "Must NOT Have" absent:
  - [ ] Non-gateway nodes don't enable WiFi
  - [ ] No blocking serial reads
  - [ ] Core algorithms untouched
- [ ] Stability test passed (4 hours, heap stable, no WDT resets)
