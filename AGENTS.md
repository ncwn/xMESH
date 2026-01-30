# xMESH AI Agent Guide

**Last Updated**: 2026-01-31
**Version**: 1.2.5
**Status**: All tests pass, encryption bug fixed, production ready

---

## Project Overview

xMESH is a LoRa mesh network for ESP32-S3 (Heltec V3). **104 unit tests pass**.

| Component | Lines | Status |
|-----------|-------|--------|
| xmesh-core | 1,732 | Thread-safe with FreeRTOS mutexes |
| xmesh-hal | 580 | Display + Sensors |
| xmesh-ota | 480 | ArduinoOTA + HTTP OTA |
| xmesh-security | 1,700 | AES-256-GCM, replay protection, device auth |
| firmware | 1,290 | Full integration |
| src/ (LoRaMesher fork) | 6,119 | Modified fork - see [FORK_MODIFICATIONS.md](docs/FORK_MODIFICATIONS.md) |
| **Total** | ~11,900 | |

---

## Key Files

### Libraries
```
lib/xmesh-core/
  include/xmesh/
    CostRouter.h         Multi-metric path selection
    TrickleScheduler.h   RFC 6206 adaptive scheduling
    ETXTracker.h         Link quality estimation
    GatewayBalancer.h    Load distribution + neighbor health
    MobilityDetector.h   SNR variance state machine
    RoutingAdapter.h     Thread-safe LoRaMesher bridge
  src/
    *.cpp                Implementations (all mutex-protected)

lib/xmesh-hal/
  include/xmesh/hal/
    Display.h            SSD1306 OLED driver
    Sensors.h            PMS7003 + GPS drivers
    SensorPacket.h       23-byte mesh packet
  src/
    Display.cpp          With null-guards
    Sensors.cpp          With destructors

lib/xmesh-ota/
  include/ota/
    OTAManager.h         ArduinoOTA + HTTP OTA + rollback
  src/
    OTAManager.cpp

lib/xmesh-security/
  include/xmesh/security/
    FrameCounter.h       Replay protection (32-bit counters, window validation)
    PayloadCrypto.h      AES-256-GCM encryption (12-byte nonce, 4-byte tag)
    DeviceAuth.h         Device allowlist (64 devices, MAC hash binding)
    KeyManager.h         Key storage + PBKDF2 derivation (rotation: optional via XMESH_ENABLE_KEY_ROTATION)
    SecurityManager.h    Unified facade (SecurityLevel enum)
  src/
    *.cpp                Implementations (all mutex-protected)
```

### Firmware
```
firmware/production/
  src/main.cpp           Entry point, CLI, callbacks (~1,290 LOC)
  include/config.h       All configuration constants
  include/DutyCycleBudget.h
  platformio.ini         Build config (includes [env:native] for tests)
  partitions.csv         OTA A/B slots
  test/                  Unit test infrastructure
    mocks/               FreeRTOS, Arduino, ESP-IDF mocks
    test_native/         88 unit tests for 11 modules
```

---

## Build Commands

```bash
cd /Volumes/xMESH/xMESH/firmware/production

# Note: If 'pio' is not in PATH, use: python3 -m platformio <command>

# Build for hardware
pio run

# Run unit tests (native)
pio test -e native

# Flash USB
pio run -t upload --upload-port /dev/cu.usbserial-XXXX

# Flash OTA (gateway with WiFi)
pio run -e ota -t upload --upload-port <IP>

# Monitor
pio device monitor --baud 115200
```

---

## Test Infrastructure

### Unit Tests (104 tests, 100% pass)

```bash
pio test -e native    # Runs all native tests
```

| Module | Tests | File |
|--------|-------|------|
| CostRouter | 5 | test_native/test_cost_router/ |
| TrickleScheduler | 12 | test_native/test_trickle_scheduler/ |
| ETXTracker | 6 | test_native/test_etx_tracker/ |
| GatewayBalancer | 13 | test_native/test_gateway_balancer/ |
| MobilityDetector | 8 | test_native/test_mobility_detector/ |
| Security (FrameCounter/DeviceAuth) | 17 | test_native/test_security/ |
| PayloadCrypto | 6 | test_native/test_payload_crypto/ |
| KeyManager | 10 | test_native/test_key_manager/ |
| SecurityManager | 11 | test_native/test_security_manager/ |
| Sensors | 8 | test_native/test_sensors/ |
| OTAManager | 8 | test_native/test_ota_manager/ |

### Hardware Tests

| Test | Duration | Evidence |
|------|----------|----------|
| CLI Commands | ~30 min | `.sisyphus/evidence/cli-commands-test.md` |
| Sensor Detection | ~10 min | `.sisyphus/evidence/test-results-20260130.md` |
| Mesh Formation | ~5 min | `.sisyphus/evidence/test-results-20260130.md` |
| Trickle Suppression | ~10 min | 77.8% suppression achieved |
| Failed Neighbor | ~7 min | Infrastructure verified |

### Test Procedures
- `.sisyphus/evidence/hardware-test-procedures.md` - Full procedures
- `.sisyphus/evidence/cli-commands-test.md` - CLI command checklist

---

## Critical Conventions

### DO NOT Modify Without Explicit Instruction

| Module | Reason |
|--------|--------|
| TrickleScheduler algorithm | RFC 6206 implementation |
| CostRouter cost function | Research-validated weights |
| ETXTracker calculation | Sequence-gap detection math |
| LoRaMesher core | Upstream library |

### Code Standards

| Rule | Example |
|------|---------|
| Namespace | `xmesh::` for core, `xmesh::hal::` for HAL |
| Logging | `ESP_LOGX(TAG, ...)` - never printf |
| Threading | FreeRTOS mutex for shared state (already implemented) |
| Allocation | Check nulls, use `std::nothrow` |
| Types | `uint16_t`, `int8_t` - explicit widths |

### Build Requirements

- `LM_GOD_MODE` flag required for LoRaMesher hooks
- `partitions.csv` for OTA dual-slot
- Heltec V3 board definition

---

## LoRaMesher Modifications

Minimal modifications to preserve upstream updateability:

| File | Change | Reason |
|------|--------|--------|
| `src/LoraMesher.h:1065` | `volatile bool hasReceivedMessage` | ISR flag correctness |
| `src/LoraMesher.h:316` | `deleteRoute()` declaration | Neighbor failure cleanup |
| `src/LoraMesher.cpp:793` | `deleteRoute()` implementation | Route removal |
| `src/services/RoutingTableService.*` | `removeRoute()`, `findNodeCopy()` | Thread-safe operations |

**When updating LoRaMesher**: Re-apply these modifications.

---

## Bug Fixes Applied (19 total)

All fixes from `.sisyphus/archive/xmesh-bug-fixes.md` are complete:

**Race Conditions**: 6 fixes (volatile, mutexes, copy-out pattern)
**Memory Leaks**: 3 fixes (destructors, static allocation)
**Integration Gaps**: 3 fixes (failure callbacks, cost checks)
**Error Handling**: 6 fixes (null-guards, logging, persistence)
**Encryption**: 1 fix (v1.2.5 - buffer overlap in PayloadCrypto::encrypt)

---

## Known TODOs (Future Work)

All previously unused code has been cleaned up. No remaining TODOs for core functionality.

| Location | Description | Priority |
|----------|-------------|----------|
| Mesh OTA Propagation | Not started | Future |
| CI/CD Pipeline | Not started | Future |

---

## Unused Code Cleanup (v1.2.4)

The following unused code was removed or consolidated:

### Bug Fixes (v1.2.4)
| Fix | Location | Impact |
|-----|----------|--------|
| **LoRa config wiring** | main.cpp | Radio now uses config.h values (freq, bw, sf, cr, power) instead of library defaults |
| `FrameCounter::findPeer()` | FrameCounter.h/cpp | Removed unused private function |
| `LOAD_SWITCH_THRESHOLD` | config.h | Removed unused constant |
| `MAX_GATEWAY_CANDIDATES` | config.h | Removed unused constant |
| `IS_GATEWAY_NODE` | config.h | Removed unused constant |
| `NODE_ADDRESS` | config.h | Removed unused constant |

### Previously Removed (v1.2.3)
| Function | Location | Reason |
|----------|----------|--------|

### Consolidated Under #ifdef XMESH_ENABLE_KEY_ROTATION
| Function | Notes |
|----------|-------|
| `KeyManager::rotateKey()` | Only useful with rotation enabled |
| `KeyManager::getPreviousVersion()` | Only useful with rotation enabled |
| `KeyManager::generateRandomKey()` | Only useful with rotation enabled |

### Preserved (Intentionally Unused)
- `DEVICE_FLAG_*` constants - Reserved for future use
- Write-only struct fields - Wire format stability
- Config constants in config.h - User configuration templates

---

## Testing Quick Reference

### Verify Build
```bash
cd firmware/production && pio run
# Expected: SUCCESS with 0 errors
```

### Run Unit Tests
```bash
cd firmware/production && pio test -e native
# Expected: 88/88 tests pass
```

### Hardware Test (3 nodes)
1. Flash all nodes
2. Power on simultaneously
3. Monitor: routes form in <60s
4. Verify Trickle suppression in logs
5. Run: `status`, `neighbors`, `routes` on each

---

## Serial Commands

```
status              Node overview
neighbors           Mesh neighbors  
routes              Routing table
gateway on/off      Toggle gateway mode
wifi SSID PASS      Connect WiFi
wifi status         Show WiFi state
wifi scan           Scan available networks
send XXXX           Send test packet (hex address)
sensors status      Detection + values
sensors detect      Re-run detection
sensors send        Force mesh TX
sensors read        Force immediate sensor read
sensors power on/off  Control sensor power
mqtt <broker>       Set MQTT broker
mqtt status         Connection state
reset trickle       Reset to I_min
mobility on/off     Toggle detection
mobility simulate   Force mobility state change
emergency           Trigger emergency state
dutycycle           Show duty cycle usage
security status     Show security level, mode, device count
security level N    Set level (0=NONE, 1=AUTH_ONLY, 2=ENCRYPTED, 3=FULL)
security key PASS   Derive encryption key from password
security devices    Show authorized device count
security add XXXX   Add device to allowlist
security remove XXXX Remove device from allowlist
security mode N     Set auth mode (0=OPEN, 1=LEARNING, 2=ALLOWLIST)
ota status          Show OTA state, version, progress
ota check           Check for updates (configured URL)
ota check <url>     Check for updates from URL
ota url <url>       Set firmware URL for HTTP OTA
ota update          Start HTTP update (configured URL)
ota update <url>    Start HTTP update from URL
ota abort           Abort OTA update in progress
help                List all commands
```

---

## Architecture Diagram

```
+-------------------+
|   main.cpp        |  CLI, callbacks, integration
+--------+----------+
         |
+--------v----------+     +------------------+     +------------------+
|   xmesh-core      |     |   xmesh-hal      |     |  xmesh-security  |
| Trickle, Cost,    |     | Display, Sensors |     | AES-GCM, Replay, |
| ETX, Gateway,     |     |                  |     | DeviceAuth, Keys |
| Mobility, Adapter |     |                  |     |                  |
+--------+----------+     +--------+---------+     +--------+---------+
         |                         |                        |
+--------v-------------------------v------------------------v---------+
|              src/ - LoRaMesher Fork                                  |
|         (Physical/MAC Layer, 6,119 LOC)                              |
|         See: docs/FORK_MODIFICATIONS.md                              |
+----------------------------------------------------------------------+
```

---

## Session Continuity

This file serves as the AGENTS.md for AI assistants working on xMESH.

**Key Memory Points**:
1. Codebase is PRODUCTION READY - all tests pass
2. All 19 bug fixes complete
3. 104 unit tests exist and pass (pio test -e native) - expanded from 88 on 2026-01-31
4. 282 total tests verified (unit + hardware) - see `.sisyphus/evidence/test-coverage-final.md`
5. Thread-safety via FreeRTOS mutexes in all xmesh-core modules
6. Security layer complete (AES-256-GCM, replay protection, device auth)
7. HTTP OTA with signature verification complete
8. Build always succeeds with `pio run`
9. **Core code fully integrated** (security TX/RX, gateway load balancing, HTTP OTA CLI, periodic cleanup)
10. Key rotation disabled by default (enable via `-DXMESH_ENABLE_KEY_ROTATION` build flag)

---

## Workflow Files

| File | Purpose |
|------|---------|
| `.sisyphus/boulder.json` | Active plan tracker (COMPLETE) |
| `.sisyphus/plans/` | Current/active plans |
| `.sisyphus/archive/` | Completed plans |
| `.sisyphus/evidence/` | Test results |
| `.sisyphus/PROJECT_MEMORY.md` | Full project state |

---

## Test Results Summary

| Category | Tests | Passed | Rate |
|----------|-------|--------|------|
| Unit Tests | 104 | 104 | 100% |
| CLI Commands | 27 | 27 | 100% |
| Security Tests | 8 | 8 | 100% |
| Sensor Tests | 6 | 6 | 100% |
| Mesh Tests | 3 | 3 | 100% |
| Trickle Tests | 3 | 3 | 100% |
| Mobility Tests | 5 | 5 | 100% |
| Gateway Tests | 4 | 4 | 100% |
| OTA Tests | 3 | 3 | 100% |
| Duty Cycle | 3 | 3 | 100% |
| **TOTAL** | **178** | **178** | **100%** |

---

**xMESH** - Production LoRa mesh. Tested. Bug-fixed. Ready for deployment.
