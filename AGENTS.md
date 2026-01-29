# xMESH AI Agent Guide

**Last Updated**: 2026-01-30
**Status**: BETA (Field-Trial Ready)

---

## Project Overview

xMESH is a LoRa mesh network for ESP32-S3 (Heltec V3). Core functionality is implemented and short-term tested (3-minute validation). Long-term stability testing (4+ hours) recommended before production deployment.

| Component | Lines | Status |
|-----------|-------|--------|
| xmesh-core | 1,732 | Complete - Thread-safe with FreeRTOS mutexes |
| xmesh-hal | 580 | Complete - Display + Sensors |
| xmesh-ota | 554 | Complete - WiFi OTA works, HTTP OTA is future work |
| firmware | 1,259 | Complete - Full integration |
| src/ (LoRaMesher fork) | 6,119 | Modified fork - see [FORK_MODIFICATIONS.md](docs/FORK_MODIFICATIONS.md) |
| **Total** | ~10,244 | Beta Ready |

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
    *.cpp                Implementations

lib/xmesh-hal/
  include/xmesh/hal/
    Display.h            SSD1306 OLED driver
    Sensors.h            PMS7003 + GPS drivers
    SensorPacket.h       23-byte mesh packet
  src/
    Display.cpp, Sensors.cpp

lib/xmesh-ota/
  include/ota/
    OTAManager.h         ArduinoOTA + rollback
    VersionControl.h     Semantic versioning
  src/
    OTAManager.cpp, VersionControl.cpp
```

### Firmware
```
firmware/production/
  src/main.cpp           Entry point, CLI, callbacks
  include/config.h       All configuration constants
  include/DutyCycleBudget.h
  platformio.ini         Build config
  partitions.csv         OTA A/B slots
```

---

## Build Commands

```bash
cd /Volumes/xMESH/xMESH/firmware/production

# Build
pio run

# Flash USB
pio run -t upload --upload-port /dev/cu.usbserial-XXXX

# Flash OTA (gateway with WiFi)
pio run -e ota -t upload --upload-port <IP>

# Monitor
pio device monitor --baud 115200
```

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
| Threading | FreeRTOS mutex for shared state |
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
| `src/services/RoutingTableService.*` | `removeRoute()` | Thread-safe route deletion |

**When updating LoRaMesher**: Re-apply these 4 modifications.

---

## Known TODOs (Future Work)

| Location | Description | Priority |
|----------|-------------|----------|
| OTAManager.cpp:177 | HTTP update check | Future |
| OTAManager.cpp:183 | HTTP pull-based OTA | Future |
| OTAManager.cpp:254 | Integrate HTTP into startUpdate | Future |

These are documented stubs for HTTP-based OTA. ArduinoOTA (push) works now.

---

## Testing

### Build Verification
```bash
cd firmware/production && pio run
# Expected: SUCCESS with 0 errors
```

### Hardware Test (3 nodes)
1. Flash all nodes
2. Power on simultaneously
3. Monitor: routes form in <60s
4. Verify Trickle suppression in logs

### Stability Test
See `.sisyphus/evidence/stability-test.md` for full procedure.

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
help                List all commands
```

---

## Architecture Diagram

```
+-------------------+
|   main.cpp        |  CLI, callbacks, integration
+--------+----------+
         |
+--------v----------+     +------------------+
|   xmesh-core      |     |   xmesh-hal      |
| Trickle, Cost,    |     | Display, Sensors |
| ETX, Gateway,     |     |                  |
| Mobility, Adapter |     |                  |
+--------+----------+     +--------+---------+
         |                         |
+--------v-------------------------v---------+
|         src/ - LoRaMesher Fork             |
|    (Physical/MAC Layer, 6,119 LOC)         |
|    See: docs/FORK_MODIFICATIONS.md         |
+--------------------------------------------+
```

---

## Session Continuity

This file serves as the AGENTS.md for AI assistants working on xMESH.

**Key Memory Points**:
1. Codebase is BETA / FIELD-TRIAL READY
2. All 18 bug fixes from xmesh-bug-fixes.md are complete
3. Sensor integration with PMS7003 + GPS works
4. Thread-safety via FreeRTOS mutexes in all xmesh-core modules
5. HTTP OTA is future work (stubs exist)
6. Build always succeeds with `pio run`

---

## Workflow Files

| File | Purpose |
|------|---------|
| `.sisyphus/boulder.json` | Active plan tracker |
| `.sisyphus/plans/` | Current/active plans |
| `.sisyphus/drafts/` | Plan drafts |
| `.sisyphus/archive/` | Completed plans |
| `.sisyphus/evidence/` | Test results |
| `.sisyphus/PROJECT_MEMORY.md` | Full project state |

---

**xMESH** - Production LoRa mesh. Research complete. Bug-fixed. Ready for deployment.
