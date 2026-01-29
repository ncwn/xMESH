# xMESH: Production LoRa Mesh Documentation (AI-Optimized)

## Project Overview
xMESH is a professional-grade LoRa mesh network designed for production IoT deployments. It is a modular refactor of a research prototype ("Protocol 3") that focuses on scalability, reliability, and efficient routing in resource-constrained environments.

### Core Objectives
- **Scalability**: Sublinear control overhead growth via adaptive scheduling.
- **Reliability**: Multi-metric cost routing and zero-overhead ETX tracking.
- **Production-Ready**: Clean separation of concerns (Core, HAL, OTA) and minimized main firmware logic.

## Module List & File Locations

### 1. xmesh-core (`lib/xmesh-core/`)
Implementation of the Protocol 3 routing stack (~1,506 lines).
- Headers: `lib/xmesh-core/include/xmesh/`
- Sources: `lib/xmesh-core/src/`
- `CostRouter.h/cpp`: Multi-metric path selection logic (166 lines).
- `TrickleScheduler.h/cpp`: RFC 6206 adaptive HELLO scheduling (295 lines).
- `ETXTracker.h/cpp`: Zero-overhead Link Quality Estimation (271 lines).
- `GatewayBalancer.h/cpp`: Active load sharing and neighbor health monitoring (484 lines).
- `MobilityDetector.h/cpp`: SNR variance-based mobility state machine (240 lines).
- `MeshConfig.h`: Routing parameter definitions (50 lines).

### 2. xmesh-hal (`lib/xmesh-hal/`)
Hardware Abstraction Layer for Heltec WiFi LoRa 32 V3 (~618 lines).
- Headers: `lib/xmesh-hal/include/xmesh/hal/`
- Sources: `lib/xmesh-hal/src/`
- `Display.h/cpp`: SSD1306 OLED drivers and status rendering (248 lines).
- `Sensors.h/cpp`: PMS7003 + GPS drivers with power management (296 lines).
- `SensorPacket.h`: 23-byte mesh packet structure for sensor data (44 lines).

### 3. xmesh-ota (`lib/xmesh-ota/`)
ESP-IDF native Over-The-Air update integration (~572 lines).
- Headers: `lib/xmesh-ota/include/ota/`
- Sources: `lib/xmesh-ota/src/`
- `OTAManager.h/cpp`: ArduinoOTA + rollback safety (407 lines).
- `VersionControl.h/cpp`: Semantic versioning utilities (147 lines).

### 4. Production Firmware (`firmware/production/`)
Reference implementation for field deployment.
- `main.cpp`: System entry point and callback wiring.
- `config.h`: Production configuration constants.
- `platformio.ini`: Build configuration and environment flags.
- `partitions.csv`: Custom partition scheme for OTA support.

## Common Development Tasks

| Task | Command / Instructions |
|------|------------------------|
| **Build** | `pio run` in `firmware/production` |
| **Flash (USB)** | `pio run -t upload` |
| **Monitor** | `pio device monitor --baud 115200` |
| **Update (OTA)**| `pio run -t upload --upload-port <IP_ADDRESS>` |
| **Clean** | `pio run -t clean` |
| **Log Search** | `grep -r "ESP_LOGE" .` to find error logs |

## Key Conventions

1. **Algorithm Integrity**: Do NOT tune or modify core routing algorithms (Trickle, Cost Function) without explicit instruction.
2. **Modular HAL**: Hardware-specific logic belongs in `xmesh-hal`. `xmesh-core` MUST remain hardware-agnostic.
3. **No Interactive Input**: Use `platformio.ini` build flags or `config.h` constants for all configuration.
4. **Error Handling**: Use `ESP_LOGX` macros for all logging. Avoid raw `printf`.
5. **Namespaces**: All code must reside within the `xmesh` namespace. HAL code in `xmesh::hal`.

## Architecture & Data Flows

Detailed documentation of system architecture, data flows (HELLO, Cost, ETX), and algorithm summaries can be found in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Deployment & Maintenance

Step-by-step flashing procedures, gateway configuration, and troubleshooting are documented in [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md).

## OTA Architecture & Safety

- **Trigger**: WiFi-based via `ArduinoOTA`.
- **Partition Scheme**: Dual-slot OTA (4MB Flash): `app0`/`app1` (1.9MB each).
- **Rollback**: Automatic rollback triggered after 3 consecutive boot failures (tracked in NVS).
- **Verification**: App must call `OTAManager::markAppValid()` to reset failure counter.

## Build Flags
Ensure `board_build.partitions = partitions.csv` is set in `platformio.ini` for OTA support.
Use `-DLM_GOD_MODE` to enable advanced xMESH features in LoRaMesher.
