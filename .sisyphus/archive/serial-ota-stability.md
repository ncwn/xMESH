# Serial Commands, OTA Updates, and Stability Testing for xMESH

## TL;DR

> **Quick Summary**: Add runtime serial command interface with NVS persistence, enable WiFi-based OTA for gateway nodes, and validate mesh stability over 4-hour test.
> 
> **Deliverables**:
> - Serial command parser in main.cpp with `gateway`, `status`, `reset trickle`, `wifi`, `help` commands
> - NVS-backed settings persistence (gateway mode, WiFi credentials)
> - WiFi connection manager for OTA-enabled gateway nodes
> - OTAManager integration with existing infrastructure
> - 4-hour stability test procedure with pass/fail criteria
> 
> **Estimated Effort**: Medium (2-3 days)
> **Parallel Execution**: YES - 2 waves
> **Critical Path**: Task 1 → Task 2 → Task 3 → Task 5 → Task 7

---

## Context

### Original Request
Implement serial commands, OTA updates, and stability testing for xMESH LoRa mesh network on 3 Heltec WiFi LoRa 32 V3 nodes. Priority order: Serial → OTA → Stability.

### Interview Summary
**Key Discussions**:
- Serial persistence: NVS from the start (user chose robustness over simplicity)
- WiFi credentials: Set via `wifi SSID PASSWORD` command, saved to NVS
- OTA scope: Gateway node only (reduces WiFi overhead on mesh nodes)
- Stability duration: 4 hours (appropriate for development stage)

**Research Findings**:
- `main.cpp` is 149 lines, output-only Serial logging, no input handling
- `OTAManager.cpp` has full ArduinoOTA integration but lacks WiFi.begin()
- `IS_GATEWAY_NODE` is `constexpr` in config.h - must become runtime variable
- `TrickleScheduler::reset()` and `GatewayBalancer::setIsGateway()` APIs ready
- NVS namespace "xmesh_ota" already used by OTAManager

### Metis Review
**Identified Gaps** (addressed):
- NVS namespace collision: Use distinct "xmesh_cfg" namespace
- Gateway state sync: Must call both `gatewayBalancer.setIsGateway()` AND `LoraMesher::addGatewayRole()`
- WiFi credential parsing: Handle quoted strings for spaces in SSID/password
- OTA blocking: Ignore serial commands when OTA in progress
- Heap threshold: ESP32-S3 warning at <50KB free, critical at <20KB

---

## Work Objectives

### Core Objective
Enable runtime configuration and remote firmware updates for xMESH gateway nodes while validating long-term mesh stability.

### Concrete Deliverables
- `firmware/production/src/main.cpp` with serial command parser (~100 lines added)
- `firmware/production/src/SettingsManager.h/cpp` for NVS operations (~80 lines)
- `firmware/production/src/WiFiManager.h/cpp` for WiFi connection (~60 lines)
- `.sisyphus/evidence/stability-test-procedure.md` documentation
- Git commits with conventional commit messages

### Definition of Done
- [ ] Serial commands respond within 100ms
- [ ] `wifi` command connects and reports IP within 30 seconds
- [ ] OTA upload succeeds via `pio run -t upload --upload-port <IP>`
- [ ] 4-hour stability test passes with zero watchdog resets

### Must Have
- All 5 serial commands working: `gateway on/off`, `status`, `reset trickle`, `wifi`, `help`
- NVS persistence for gateway mode and WiFi credentials
- WiFi connection with timeout and error reporting
- OTA update capability on gateway node
- Documented stability test procedure

### Must NOT Have (Guardrails)
- NO remote CLI over LoRa mesh (keep serial-only for this phase)
- NO web dashboard or captive portal for configuration
- NO HTTP OTA or mesh OTA propagation
- NO OLED display initialization (out of scope)
- NO changes to core routing algorithms (Trickle, CostRouter, ETXTracker)
- NO modifications to LoRaMesher library internals

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: NO (no test framework currently)
- **User wants tests**: Manual verification (development phase)
- **Framework**: None - use serial output and automated bash verification

### Automated Verification Approach

Each TODO includes EXECUTABLE verification procedures that agents can run directly:

| Type | Verification Tool | Automated Procedure |
|------|------------------|---------------------|
| Serial Commands | PlatformIO monitor + expect scripts | Send command via serial, capture output, validate patterns |
| WiFi/OTA | curl + ping | Verify IP reachable, test OTA upload |
| Stability | Serial logging + grep | Parse 4-hour log for errors, heap values |

**Evidence Requirements**:
- Serial output captured and compared against expected patterns
- OTA upload exit code verified (0 = success)
- Heap/watchdog logs parsed for anomalies

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately):
├── Task 1: SettingsManager (NVS persistence)
└── Task 6: Stability test documentation

Wave 2 (After Task 1):
├── Task 2: Serial command parser
├── Task 4: WiFiManager implementation
└── (Task 6 continues independently)

Wave 3 (After Tasks 2, 4):
├── Task 3: Gateway role integration
└── Task 5: OTA integration

Wave 4 (After Wave 3):
└── Task 7: End-to-end verification

Wave 5 (After Task 7):
└── Task 8: Execute 4-hour stability test

Critical Path: Task 1 → Task 2 → Task 3 → Task 5 → Task 7 → Task 8
Parallel Speedup: ~35% faster than sequential
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 1 | None | 2, 3, 4, 5 | 6 |
| 2 | 1 | 3, 5 | 4, 6 |
| 3 | 1, 2 | 5, 7 | 4, 6 |
| 4 | 1 | 5 | 2, 3, 6 |
| 5 | 3, 4 | 7 | 6 |
| 6 | None | 8 | 1, 2, 3, 4, 5 |
| 7 | 5 | 8 | None |
| 8 | 6, 7 | None | None (final) |

### Agent Dispatch Summary

| Wave | Tasks | Recommended Dispatch |
|------|-------|---------------------|
| 1 | 1, 6 | Both run_in_background=true |
| 2 | 2, 4 | Both run_in_background=true after Task 1 completes |
| 3 | 3, 5 | Sequential after dependencies |
| 4 | 7 | run_in_background=false (needs interactive serial) |
| 5 | 8 | Long-running, may need tmux session |

---

## TODOs

- [ ] 1. Create SettingsManager for NVS persistence

  **What to do**:
  - Create `firmware/production/src/SettingsManager.h` with interface
  - Create `firmware/production/src/SettingsManager.cpp` with implementation
  - Use Arduino Preferences library (not raw NVS) for simplicity
  - Namespace: `"xmesh_cfg"` (distinct from OTAManager's `"xmesh_ota"`)
  - Keys: `"gateway"` (bool), `"wifi_ssid"` (string, 32 chars max), `"wifi_pass"` (string, 64 chars max)
  - Implement: `begin()`, `loadSettings()`, `saveSettings()`, `isGateway()`, `setGateway(bool)`, `getWiFiSSID()`, `getWiFiPassword()`, `setWiFiCredentials(ssid, pass)`, `resetToDefaults()`
  - Add corruption detection: if Preferences.begin() fails, clear and reinitialize

  **Must NOT do**:
  - Do NOT use raw nvs_* functions (use Preferences wrapper)
  - Do NOT use "xmesh_ota" namespace (reserved for OTAManager)
  - Do NOT store any routing parameters (those stay in config.h)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Self-contained module, clear interface, <100 lines
  - **Skills**: None required
    - ESP32 Preferences is standard Arduino API

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 6)
  - **Blocks**: Tasks 2, 3, 4, 5
  - **Blocked By**: None (can start immediately)

  **References**:

  **Pattern References**:
  - `lib/xmesh-ota/src/OTAManager.cpp:20-60` - NVS usage pattern with namespace, open, get/set, commit, close
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h:177-188` - Simple getter/setter pattern for isGateway

  **API/Type References**:
  - Arduino Preferences API: `begin(namespace, readOnly)`, `putString()`, `getString()`, `putBool()`, `getBool()`, `end()`

  **External References**:
  - Arduino-ESP32 Preferences: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html

  **Acceptance Criteria**:
  
  ```bash
  # Agent builds and verifies compilation:
  cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | tail -20
  # Assert: "SUCCESS" in output
  # Assert: No errors mentioning SettingsManager
  ```

  **Commit**: YES
  - Message: `feat(settings): add SettingsManager for NVS persistence`
  - Files: `firmware/production/src/SettingsManager.h`, `firmware/production/src/SettingsManager.cpp`
  - Pre-commit: `pio run` must succeed

---

- [ ] 2. Implement serial command parser in main.cpp

  **What to do**:
  - Add non-blocking serial input handling to `loop()` function
  - Use line-based parsing with `Serial.readStringUntil('\n')`
  - Command buffer: 128 bytes max, trim whitespace
  - Block commands if OTA in progress (check `OTAManager::getState() != IDLE`)
  - Implement commands:
    - `help` - List available commands
    - `status` - Show node address, gateway mode, neighbors, routing table size, heap free, uptime
    - `reset trickle` - Call `trickle.reset()` and confirm
  - Add `#include "SettingsManager.h"` and create global instance
  - Initialize SettingsManager in `setup()` before other components
  - Load gateway setting from NVS and apply to `gatewayBalancer.setIsGateway()`

  **Must NOT do**:
  - Do NOT create a separate FreeRTOS task (keep in loop for simplicity)
  - Do NOT implement gateway or wifi commands yet (Task 3 and 5)
  - Do NOT use blocking Serial.readString() without timeout
  - Do NOT modify the existing loop timing (keep 1s delay)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Straightforward serial parsing, pattern well-documented
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential after Task 1
  - **Blocks**: Tasks 3, 5
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - `firmware/production/src/main.cpp:116-148` - Current loop() structure to extend
  - `firmware/production/src/main.cpp:68-114` - Current setup() structure

  **API References**:
  - `lib/xmesh-core/include/xmesh/TrickleScheduler.h:67` - `reset()` method signature
  - `lib/xmesh-core/include/xmesh/TrickleScheduler.h:95-105` - Status getters
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h:162` - `getNeighborCount()`
  - `lib/xmesh-core/include/xmesh/ETXTracker.h` - `getNumTrackedLinks()`

  **External References**:
  - Arduino Serial: `Serial.available()`, `Serial.readStringUntil()`, `String.trim()`

  **Acceptance Criteria**:
  
  ```bash
  # Agent builds firmware:
  cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | tail -20
  # Assert: "SUCCESS" in output

  # Agent flashes to one node and tests via serial monitor:
  # (Manual verification during Task 7)
  # Expected: typing "help" returns list of commands
  # Expected: typing "status" returns node info with Address, Gateway mode, Neighbors count
  ```

  **Commit**: YES
  - Message: `feat(serial): add command parser with help, status, reset trickle`
  - Files: `firmware/production/src/main.cpp`
  - Pre-commit: `pio run` must succeed

---

- [ ] 3. Implement gateway role toggle command

  **What to do**:
  - Add `gateway on` and `gateway off` commands to parser
  - On `gateway on`:
    1. Call `gatewayBalancer.setIsGateway(true)`
    2. Call `LoraMesher::getInstance().addGatewayRole()`
    3. Call `settingsManager.setGateway(true)` to persist
    4. Print confirmation with current state
  - On `gateway off`:
    1. Call `gatewayBalancer.setIsGateway(false)`
    2. Call `LoraMesher::getInstance().removeGatewayRole()`
    3. Call `settingsManager.setGateway(false)` to persist
    4. Print confirmation
  - On `gateway` (no argument): Show current gateway status
  - Update `status` command to show persisted vs runtime gateway state

  **Must NOT do**:
  - Do NOT restart the device on gateway change (hot-swap is supported)
  - Do NOT modify LoRaMesher library files
  - Do NOT implement WiFi enable/disable here (that's Task 5)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple command addition, APIs already identified
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 4)
  - **Blocks**: Task 5, 7
  - **Blocked By**: Tasks 1, 2

  **References**:

  **Pattern References**:
  - `firmware/production/src/main.cpp:86` - Current `gatewayBalancer.setIsGateway(IS_GATEWAY_NODE)` call

  **API References**:
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h:181` - `setIsGateway(bool)`
  - `lib/xmesh-core/include/xmesh/GatewayBalancer.h:187` - `getIsGateway()`
  - LoRaMesher API: `addGatewayRole()`, `removeGatewayRole()` (from LoraMesher.h)

  **Acceptance Criteria**:
  
  ```bash
  # Agent builds firmware:
  cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | tail -20
  # Assert: "SUCCESS" in output

  # Manual verification:
  # 1. Type "gateway" → shows current state
  # 2. Type "gateway on" → confirms enabled
  # 3. Reboot node → gateway mode persists (verify via "status")
  ```

  **Commit**: YES
  - Message: `feat(serial): add gateway on/off command with NVS persistence`
  - Files: `firmware/production/src/main.cpp`
  - Pre-commit: `pio run` must succeed

---

- [ ] 4. Implement WiFiManager for gateway nodes

  **What to do**:
  - Create `firmware/production/src/WiFiManager.h` with interface
  - Create `firmware/production/src/WiFiManager.cpp` with implementation
  - Implement:
    - `begin()` - Initialize WiFi in STA mode (don't connect yet)
    - `connect(ssid, password, timeoutMs=30000)` - Blocking connect with timeout
    - `disconnect()` - Disconnect and disable WiFi radio
    - `isConnected()` - Check connection status
    - `getIP()` - Return IP address as String
    - `getStatus()` - Return human-readable status string
  - WiFi power saving: Use `WiFi.setSleep(true)` to reduce interference with LoRa
  - Connection timeout: 30 seconds default, return false on failure
  - Do NOT auto-reconnect (let serial command trigger reconnect)

  **Must NOT do**:
  - Do NOT create WiFi access point (AP mode)
  - Do NOT implement captive portal
  - Do NOT block mesh operation during WiFi connect
  - Do NOT auto-start WiFi on boot (wait for explicit command)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard ESP32 WiFi patterns, <80 lines
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 2, 3)
  - **Blocks**: Task 5
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - `lib/xmesh-ota/src/OTAManager.cpp:85-99` - ArduinoOTA callback setup pattern

  **External References**:
  - ESP32 WiFi: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
  - ArduinoOTA BasicOTA example pattern

  **Acceptance Criteria**:
  
  ```bash
  # Agent builds firmware:
  cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | tail -20
  # Assert: "SUCCESS" in output
  # Assert: No errors mentioning WiFiManager
  ```

  **Commit**: YES
  - Message: `feat(wifi): add WiFiManager for gateway WiFi connection`
  - Files: `firmware/production/src/WiFiManager.h`, `firmware/production/src/WiFiManager.cpp`
  - Pre-commit: `pio run` must succeed

---

- [ ] 5. Integrate WiFi command and OTA updates

  **What to do**:
  - Add `wifi` command to serial parser:
    - `wifi <SSID> <PASSWORD>` - Save credentials and connect
    - `wifi status` - Show connection status and IP
    - `wifi disconnect` - Disconnect WiFi
    - Handle quoted strings: `wifi "My Network" "pass 123"` for spaces
  - On successful `wifi` connect:
    1. Save credentials via `settingsManager.setWiFiCredentials()`
    2. Initialize OTAManager: `otaManager.begin()`
    3. Print IP address and OTA readiness
  - Add `#include "WiFiManager.h"` and `#include <ota/OTAManager.h>`
  - Create global instances for WiFiManager and OTAManager
  - Add `otaManager.process()` call in loop() (only if WiFi connected)
  - Add `otaManager.markAppValid()` call in setup() after successful init
  - Block serial commands during OTA: check `otaManager.getState() != OTAState::IDLE`

  **Must NOT do**:
  - Do NOT auto-connect WiFi on boot (user must issue command)
  - Do NOT enable WiFi on non-gateway nodes
  - Do NOT implement HTTP OTA (use ArduinoOTA only)

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Integration task touching multiple modules, requires careful state management
  - **Skills**: None required
    - All APIs are standard Arduino/ESP-IDF

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (critical path)
  - **Blocks**: Task 7
  - **Blocked By**: Tasks 3, 4

  **References**:

  **Pattern References**:
  - `lib/xmesh-ota/src/OTAManager.cpp:25-169` - Full OTAManager implementation
  - `lib/xmesh-ota/include/ota/OTAManager.h:51-124` - OTAManager public interface

  **API References**:
  - `lib/xmesh-ota/include/ota/OTAManager.h:57` - `begin()` method
  - `lib/xmesh-ota/include/ota/OTAManager.h:76` - `process()` method (call in loop)
  - `lib/xmesh-ota/include/ota/OTAManager.h:100` - `markAppValid()` method
  - `lib/xmesh-ota/include/ota/OTAManager.h:82` - `getState()` for blocking check

  **Acceptance Criteria**:
  
  ```bash
  # Agent builds firmware:
  cd /Volumes/xMESH/xMESH/firmware/production && pio run 2>&1 | tail -20
  # Assert: "SUCCESS" in output

  # Manual verification (Task 7):
  # 1. Flash to gateway node
  # 2. Type "gateway on"
  # 3. Type "wifi YourSSID YourPassword"
  # 4. Wait for IP address output
  # 5. Note IP address for OTA test
  ```

  **Commit**: YES
  - Message: `feat(ota): integrate WiFi command and OTAManager for gateway nodes`
  - Files: `firmware/production/src/main.cpp`
  - Pre-commit: `pio run` must succeed

---

- [ ] 6. Document stability test procedure

  **What to do**:
  - Create `.sisyphus/evidence/stability-test-procedure.md`
  - Document test setup:
    - 3 nodes, one gateway, indoor near-range
    - All nodes connected via USB for logging
    - Serial monitor capturing output to files
  - Document monitoring metrics:
    - Free heap (via `esp_get_free_heap_size()`) - log every 60s
    - Watchdog resets (count in output)
    - Routing table size stability
    - Trickle transmit/suppress counts
    - Neighbor count stability
  - Define pass criteria:
    - Zero watchdog resets over 4 hours
    - Heap variance < 5% (no steady leak)
    - Minimum heap never below 20KB
    - Routing table size stable after initial convergence
  - Include commands to run:
    - `pio device monitor --baud 115200 --port /dev/cu.usbserial-X | tee node-X.log`
    - Post-test analysis: `grep -c "WATCHDOG" node-X.log`
  - Add heap logging to `status` command output specification

  **Must NOT do**:
  - Do NOT implement automated test harness (manual for now)
  - Do NOT require specific WiFi for the test (mesh-only is fine)

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation task, no code changes
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (independent of all code tasks)
  - **Blocks**: Task 8
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `firmware/production/src/main.cpp:132-144` - Current periodic status logging pattern

  **Documentation References**:
  - `README.md` - Project overview for context
  - `AGENTS.md` - Build commands reference

  **Acceptance Criteria**:
  
  ```bash
  # Agent verifies file exists and has content:
  wc -l /Volumes/xMESH/xMESH/.sisyphus/evidence/stability-test-procedure.md
  # Assert: > 50 lines
  
  # Agent verifies key sections present:
  grep -c "Pass Criteria" /Volumes/xMESH/xMESH/.sisyphus/evidence/stability-test-procedure.md
  # Assert: >= 1
  ```

  **Commit**: YES
  - Message: `docs(test): add 4-hour stability test procedure`
  - Files: `.sisyphus/evidence/stability-test-procedure.md`
  - Pre-commit: None required

---

- [ ] 7. End-to-end verification and OTA test

  **What to do**:
  - Flash firmware to all 3 nodes via USB
  - Verify mesh formation (nodes discover each other)
  - Test all serial commands on each node:
    - `help` - Lists commands
    - `status` - Shows node info
    - `reset trickle` - Resets interval
    - `gateway on/off` - Toggles and persists
  - On gateway node:
    - `wifi <SSID> <PASSWORD>` - Connect to WiFi
    - Verify IP is assigned
    - Test OTA upload: `pio run -t upload --upload-port <IP>`
    - Verify node reboots and runs new firmware
  - Verify OTA rollback:
    - Create intentionally broken firmware (infinite loop in setup)
    - Upload via OTA
    - Verify automatic rollback after 3 failed boots
  - Document results in `.sisyphus/evidence/verification-results.md`

  **Must NOT do**:
  - Do NOT skip OTA rollback verification
  - Do NOT proceed to stability test if any command fails

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Hardware interaction, serial monitoring, OTA testing
  - **Skills**: [`playwright`] (if browser verification needed, otherwise none)

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (depends on all code tasks)
  - **Blocks**: Task 8
  - **Blocked By**: Tasks 5, 6

  **References**:

  **External References**:
  - PlatformIO OTA: `pio run -t upload --upload-port <IP>`

  **Acceptance Criteria**:
  
  ```bash
  # Agent runs OTA upload and verifies:
  cd /Volumes/xMESH/xMESH/firmware/production
  pio run -t upload --upload-port <GATEWAY_IP> 2>&1 | tail -10
  # Assert: "SUCCESS" or "100%" in output

  # Agent pings gateway to verify it rebooted:
  ping -c 3 <GATEWAY_IP>
  # Assert: 3 packets received
  ```

  **Evidence to Capture**:
  - [ ] Screenshot or log of successful OTA upload
  - [ ] Serial output showing all commands working
  - [ ] Rollback test log (3 failed boots → revert)

  **Commit**: YES
  - Message: `test(verify): complete end-to-end verification of serial, OTA, rollback`
  - Files: `.sisyphus/evidence/verification-results.md`
  - Pre-commit: None required

---

- [ ] 8. Execute 4-hour stability test

  **What to do**:
  - Follow procedure from Task 6 documentation
  - Start serial logging on all 3 nodes to files
  - Let mesh run for 4 hours minimum
  - Collect logs and analyze:
    - Count watchdog resets
    - Plot heap usage over time (or calculate variance)
    - Verify routing table stability
  - Document results in `.sisyphus/evidence/stability-test-results.md`
  - If test fails, document failure mode and root cause hypothesis

  **Must NOT do**:
  - Do NOT skip any of the 3 nodes
  - Do NOT interrupt test early unless critical failure

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Long-running task, requires patience and log analysis
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Final task
  - **Blocks**: None (final task)
  - **Blocked By**: Tasks 6, 7

  **References**:

  **Documentation References**:
  - `.sisyphus/evidence/stability-test-procedure.md` - Procedure to follow (created in Task 6)

  **Acceptance Criteria**:
  
  ```bash
  # Agent analyzes logs after 4 hours:
  grep -c "WATCHDOG" node-*.log
  # Assert: 0 (zero watchdog resets)

  # Agent checks heap stability:
  grep "Free Heap" node-1.log | awk '{print $NF}' | sort -n | uniq -c
  # Assert: Heap values don't show steady decline
  ```

  **Evidence to Capture**:
  - [ ] node-1.log, node-2.log, node-3.log (4 hours each)
  - [ ] Summary analysis with pass/fail determination
  - [ ] Heap usage graph or statistics

  **Commit**: YES
  - Message: `test(stability): complete 4-hour stability test with results`
  - Files: `.sisyphus/evidence/stability-test-results.md`
  - Pre-commit: None required

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `feat(settings): add SettingsManager for NVS persistence` | SettingsManager.h/cpp | `pio run` |
| 2 | `feat(serial): add command parser with help, status, reset trickle` | main.cpp | `pio run` |
| 3 | `feat(serial): add gateway on/off command with NVS persistence` | main.cpp | `pio run` |
| 4 | `feat(wifi): add WiFiManager for gateway WiFi connection` | WiFiManager.h/cpp | `pio run` |
| 5 | `feat(ota): integrate WiFi command and OTAManager for gateway nodes` | main.cpp | `pio run` |
| 6 | `docs(test): add 4-hour stability test procedure` | stability-test-procedure.md | N/A |
| 7 | `test(verify): complete end-to-end verification of serial, OTA, rollback` | verification-results.md | N/A |
| 8 | `test(stability): complete 4-hour stability test with results` | stability-test-results.md | N/A |

---

## Success Criteria

### Verification Commands
```bash
# Build verification
cd /Volumes/xMESH/xMESH/firmware/production && pio run
# Expected: "SUCCESS"

# OTA verification (after WiFi connected)
pio run -t upload --upload-port <GATEWAY_IP>
# Expected: "100%" and successful reboot

# Stability verification
grep -c "WATCHDOG" node-*.log
# Expected: 0
```

### Final Checklist
- [ ] All 5 serial commands functional (help, status, reset trickle, gateway, wifi)
- [ ] NVS persistence verified (settings survive reboot)
- [ ] WiFi connection working on gateway node
- [ ] OTA upload successful at least once
- [ ] OTA rollback verified (3 failed boots → revert)
- [ ] 4-hour stability test passed
- [ ] All changes committed with conventional commit messages
- [ ] NO modifications to core routing algorithms
- [ ] NO OLED display code added
- [ ] NO remote CLI or web dashboard
