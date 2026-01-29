# xMESH: Production LoRa Mesh Network

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](#changelog)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#license)
[![Hardware](https://img.shields.io/badge/hardware-Heltec_V3-orange.svg)](docs/DEPLOYMENT.md)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)](#quick-start)

**xMESH** is a production-grade LoRa mesh network for IoT deployments. Built on ESP32-S3 + SX1262, it delivers reliable multi-hop routing with adaptive scheduling, sensor integration, and over-the-air updates.

---

## Quick Start

```bash
# Requirements: PlatformIO CLI, Heltec WiFi LoRa 32 V3

cd firmware/production

# Build
pio run

# Flash via USB
pio run -t upload

# Monitor
pio device monitor --baud 115200
```

---

## Features

| Feature | Description |
|---------|-------------|
| **Trickle Scheduling** | RFC 6206 adaptive HELLO timing - 40% traffic reduction in stable networks |
| **Multi-Metric Routing** | Cost = f(RSSI, SNR, ETX, Hops, Gateway Bias) with 15% hysteresis |
| **Zero-Overhead ETX** | Link quality tracking via sequence-gap detection |
| **Gateway Load Balancing** | PPM-based traffic distribution across gateways |
| **Mobility Detection** | SNR variance state machine (STATIC/MOBILE/EMERGENCY) |
| **Sensor Integration** | PMS7003 air quality + GPS with auto-detection |
| **MQTT Forwarding** | Gateway nodes publish to configurable broker |
| **WiFi OTA** | ArduinoOTA with dual-partition rollback safety |
| **Watchdog + Heap Monitor** | 30s watchdog, 15KB heap threshold alerts |

---

## Architecture

```
firmware/production/    Application layer (main.cpp, config.h)
        |
        v
lib/xmesh-core/        Routing stack (Trickle, Cost, ETX, Gateway, Mobility)
lib/xmesh-hal/         Hardware abstraction (Display, Sensors)
lib/xmesh-ota/         OTA management (ArduinoOTA, VersionControl)
        |
        v
src/                   LoRaMesher fork - Physical/MAC layer (6,119 LOC)
                       See: docs/FORK_MODIFICATIONS.md
```

---

## Repository Structure

```
xMESH/
├── firmware/production/     Production firmware
│   ├── src/main.cpp         Entry point (~1,005 LOC)
│   ├── include/config.h     Configuration constants
│   ├── platformio.ini       Build configuration
│   └── partitions.csv       OTA partition table
├── lib/
│   ├── xmesh-core/          Routing algorithms (~1,732 LOC)
│   ├── xmesh-hal/           Hardware drivers (~580 LOC)
│   └── xmesh-ota/           Update management (~554 LOC)
├── src/                     LoRaMesher fork (~6,119 LOC)
├── docs/
│   ├── ARCHITECTURE.md      System design
│   ├── DEPLOYMENT.md        Flashing guide
│   └── FORK_MODIFICATIONS.md  LoRaMesher changes
└── .sisyphus/               Development workflow
    ├── PROJECT_MEMORY.md    Project state
    └── evidence/            Test results
```

---

## Serial Commands

```
status              Node overview
neighbors           Mesh neighbors  
routes              Routing table
gateway on/off      Toggle gateway mode
wifi SSID PASS      Connect WiFi
sensors status      Sensor values
sensors detect      Re-run detection
mqtt <broker>       Set MQTT broker
dutycycle           Show duty cycle usage
help                List all commands
```

---

## Hardware

| Component | Specification |
|-----------|---------------|
| **Board** | Heltec WiFi LoRa 32 V3 |
| **MCU** | ESP32-S3 @ 240MHz |
| **Radio** | SX1262 LoRa |
| **Display** | SSD1306 OLED (128x64) |
| **Flash** | 8MB (dual OTA partitions) |
| **RAM** | 320KB |

**Supported Sensors** (auto-detected):
- PMS7003 Air Quality (UART2, SET pin GPIO 3)
- NEO-M8N GPS (UART1)

---

## Documentation

- [Architecture Guide](docs/ARCHITECTURE.md) - Algorithms and data flows
- [Deployment Guide](docs/DEPLOYMENT.md) - Hardware setup and flashing
- [Project Memory](.sisyphus/PROJECT_MEMORY.md) - Implementation status
- [Stability Test](.sisyphus/evidence/stability-test.md) - Validation procedures
- [LoRaMesher Fork](docs/FORK_MODIFICATIONS.md) - Modified library documentation

---

## Build Info

| Metric | Value |
|--------|-------|
| **Firmware Size** | ~900KB (45.8% of 1.9MB slot) |
| **RAM Usage** | 52KB (15.8% of 320KB) |
| **Code Lines** | ~10,244 total (including LoRaMesher fork) |
| **Libraries** | RadioLib, LoRaMesher (fork), PubSubClient, TinyGPSPlus, PMSerial |

---

## Changelog

### v1.0.0 (2026-01-30)
- Production-grade modular architecture (xmesh-core, xmesh-hal, xmesh-ota)
- RFC 6206 Trickle scheduling with mobility-aware adaptation
- Multi-metric cost routing with gateway load balancing
- Sensor integration with PMS7003 and GPS auto-detection
- WiFi OTA with dual-partition rollback safety
- Thread-safe modules with FreeRTOS mutexes
- 18 bug fixes (race conditions, memory leaks, integration gaps)

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

**xMESH** - Reliable LoRa mesh for production IoT.
