# xMESH PROJECT KNOWLEDGE BASE

**Generated:** 2025-01-31
**Purpose:** LoRaMesher fork with advanced routing, security, and IoT sensor mesh features

## OVERVIEW

xMESH extends LoRaMesher with multi-metric cost routing, AES encryption, Trickle scheduling, and production-ready IoT sensor firmware for ESP32 + LoRa devices. Target hardware: Heltec WiFi LoRa 32 V3.

## STRUCTURE

```
xMESH/
├── src/                  # Modified LoRaMesher core (from upstream)
├── lib/                  # xMESH extension libraries (NEW)
│   ├── xmesh-core/       # Advanced routing: CostRouter, ETXTracker, Trickle
│   ├── xmesh-security/   # AES encryption, frame counters, auth
│   ├── xmesh-hal/        # Hardware abstraction: sensors, display
│   └── xmesh-ota/        # Over-the-air updates
├── firmware/production/  # MAIN ENTRY POINT - production firmware
├── examples/             # Legacy demos from original LoRaMesher
├── mqtt-setup/           # Docker Mosquitto for gateway testing
├── .LoRaMesher-OG/       # Upstream LoRaMesher snapshot for comparison
└── utilities/            # Build/deploy scripts
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Build firmware | `firmware/production/` | `pio run` in this dir |
| Modify routing | `lib/xmesh-core/src/CostRouter.cpp` | Multi-metric cost formula |
| Add encryption | `lib/xmesh-security/src/SecurityManager.cpp` | Singleton, wraps crypto |
| Add sensors | `lib/xmesh-hal/src/Sensors.cpp` | PMS7003, GPS |
| Base mesh protocol | `src/LoraMesher.cpp` | Modified upstream |
| Run tests | `firmware/production/` | `pio test -e native` |
| Config options | `firmware/production/include/config.h` | All tunables |

## FORK MODIFICATIONS (src/ vs .LoRaMesher-OG/src/)

| File | Changes |
|------|---------|
| `BuildOptions.h` | Added `LM_GOD_MODE` (replaces `LM_TESTING`) |
| `LoraMesher.h/cpp` | Added `deleteRoute()`, `volatile hasReceivedMessage` |
| `RoutingTableService.h/cpp` | Added `costCallback`, `helloCallback`, `removeRoute()` |

## xMESH EXTENSIONS (lib/)

### xmesh-core - Advanced Routing
| Module | Purpose |
|--------|---------|
| `CostRouter` | Multi-metric cost: hops + RSSI + SNR + ETX + gateway bias |
| `ETXTracker` | Zero-overhead link quality via sequence-gap detection |
| `TrickleScheduler` | RFC 6206 adaptive HELLO (30-40% overhead reduction) |
| `GatewayBalancer` | Multi-gateway load distribution |
| `MobilityDetector` | Adaptive params based on movement |
| `RoutingAdapter` | Thread-safe routing table snapshot |

### xmesh-security - Encryption & Auth
| Module | Purpose |
|--------|---------|
| `SecurityManager` | Singleton facade (composes below modules) |
| `PayloadCrypto` | AES-CTR encryption |
| `KeyManager` | Key derivation, storage |
| `FrameCounter` | Replay protection |
| `DeviceAuth` | Device authorization list |

### xmesh-hal - Hardware Abstraction
| Module | Purpose |
|--------|---------|
| `Sensors` | PMS7003 air quality + GPS reading |
| `Display` | SSD1306 OLED (Heltec V3 internal I2C) |
| `SensorPacket` | Binary sensor data structure |

### xmesh-ota - Updates
| Module | Purpose |
|--------|---------|
| `OTAManager` | ESP-IDF OTA with rollback support |

## PRODUCTION FIRMWARE

**Entry point:** `firmware/production/src/main.cpp`

**Features:**
- All xmesh-core modules integrated
- SecurityManager with optional encryption
- PMS7003 + GPS sensors → SensorPacket broadcast
- MQTT gateway forwarding
- NVS persistent config (gateway role, WiFi, MQTT)
- OTA via ArduinoOTA or HTTP
- DutyCycleBudget for regulatory compliance

**Build:**
```bash
cd firmware/production
pio run                              # Build
pio run -t upload                    # Flash via USB
pio run -e ota -t upload --upload-port <IP>  # OTA
pio test -e native                   # Run unit tests
```

## CONVENTIONS

- **Namespace:** All xMESH code in `xmesh::` or `xmesh::security::`, `xmesh::hal::`, etc.
- **Singletons:** `LoraMesher::getInstance()`, `SecurityManager::getInstance()`
- **Thread safety:** FreeRTOS mutexes in all multi-threaded modules
- **Callbacks:** Use `RoutingTableService::setCostCalculationCallback()` to hook routing
- **Config:** All tunables in `firmware/production/include/config.h`
- **Test framework:** Unity, run on native (no hardware)

## ANTI-PATTERNS (THIS PROJECT)

- **DO NOT** modify files in `.LoRaMesher-OG/` - reference only
- **DO NOT** use `examples/` for production - they reference upstream LoRaMesher
- **DO NOT** include xmesh-security sub-modules directly - use `SecurityManager`
- **DO NOT** add build flags without updating `platformio.ini`
- **NEVER** commit WiFi/MQTT credentials (use NVS runtime config)

## BUILD COMMANDS

```bash
# Production firmware (Heltec V3)
cd firmware/production
pio run                              # Build
pio run -t upload                    # Flash USB
pio run -e native -t test            # Run tests

# MQTT broker for gateway
cd mqtt-setup
docker compose up -d

# Examples (legacy, not recommended)
cd examples/Counter
pio run
```

## DIRECTORY STATUS

| Directory | Status | Notes |
|-----------|--------|-------|
| `src/` | ACTIVE | Modified LoRaMesher core |
| `lib/xmesh-*` | ACTIVE | All modules integrated |
| `firmware/production/` | ACTIVE | Main entry point |
| `firmware/production/test/` | ACTIVE | Unit tests |
| `examples/` | LEGACY | Reference upstream, not xMESH |
| `mqtt-setup/` | ACTIVE | Docker MQTT for testing |
| `experiments/` | EMPTY | Unused |
| `docs/` | EMPTY | Unused |
| `.LoRaMesher-OG/` | REFERENCE | Upstream snapshot |
| `.sisyphus/` | META | Planning artifacts |

## HARDWARE

**Target:** Heltec WiFi LoRa 32 V3
- **LoRa:** SX1262, 923MHz (AS923)
- **Display:** Internal SSD1306 128x64 (I2C, RST=21)
- **Sensors:** PMS7003 (UART1, pins 4/5), GPS (UART2, pins 6/7)

## KNOWN ISSUES (Oracle-Validated)

### Routing Issues

| Issue | Severity | Description | Fix |
|-------|----------|-------------|-----|
| ETX not functional | HIGH | Sequence numbers use local counter, not sender's packet ID | Use `PacketHeader::id` from received packets |
| Gateway load not propagated | MEDIUM | HELLO packets don't carry load field | Add load byte to RoutePacket |
| Trickle scope limited | MEDIUM | Only paces app broadcasts, not LoRaMesher HELLOs | Hook into Hello_Task or suppress built-in |
| New-neighbor race condition | LOW | Trickle reset inconsistent on rapid neighbor changes | Add debounce timer |

### Security Issues

| Issue | Severity | Description | Fix |
|-------|----------|-------------|-----|
| 4-byte GCM tag | MEDIUM | Weak against active attackers (~2^32 forgery resistance) | Use full 16-byte tag if bandwidth permits |
| Replay after reboot | HIGH | Per-peer counters not persisted | Persist to NVS on update |
| Shared network key | MEDIUM | One compromised node compromises all | Implement pairwise keys |
| AUTH_ONLY not enforced | LOW | Production main.cpp only checks ENCRYPTED+ | Add explicit AUTH_ONLY path |

### General Limitations

- Single radio frequency (no channel hopping)
- Mesh status via serial only (no visualization)
- Gateway discovery relies on broadcast range

## NOTES

- `LM_GOD_MODE` exposes internal LoRaMesher APIs for xMESH hooks
- Security is OPTIONAL - set level via `SecurityManager::begin(level)`
- Trickle reduces HELLO overhead in stable networks (60s→600s adaptive)
- ETX uses sequence gaps - no ACK packets needed
- Gateway mode auto-enables MQTT forwarding if broker configured

## SEE ALSO

- [ARCHITECTURE.md](ARCHITECTURE.md) - Comprehensive architecture documentation
- [FORK_MODIFICATION.md](FORK_MODIFICATION.md) - LoRaMesher fork changes
