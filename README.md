# xMESH: Production LoRa Mesh Network

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#license)
[![Hardware](https://img.shields.io/badge/hardware-Heltec_V3-orange.svg)](docs/DEPLOYMENT.md)

xMESH is a professional-grade LoRa mesh network designed for production IoT deployments. It provides a modular, reliable, and scalable routing stack optimized for ESP32-S3 hardware, refactored from research-validated algorithms.

---

## Table of Contents
- [Quick Start](#quick-start)
- [Key Features](#key-features)
- [Architecture Overview](#architecture-overview)
- [Over-The-Air (OTA) Updates](#over-the-air-ota-updates)
- [Repository Structure](#repository-structure)
- [Documentation](#documentation)
- [Project Background](#project-background)
- [Contributing](#contributing)
- [License](#license)

---

## Quick Start

### 1. Requirements
- **Hardware**: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
- **Software**: [PlatformIO Core](https://platformio.org/install/cli)

### 2. Build & Flash
```bash
# Clone the repository
git clone <repository-url>
cd xMESH

# Build the production firmware
pio run

# Upload to your Heltec V3 device via USB
pio run -t upload

# Monitor serial output
pio device monitor --baud 115200
```

---

## Key Features

- **Adaptive Scheduling (Trickle)**: Implements RFC 6206 to reduce control traffic by up to 40% in stable networks while maintaining rapid convergence during topology changes.
- **Multi-Metric Cost Routing**: Path selection optimized using a weighted formula combining RSSI, SNR, ETX, Hop Count, and Gateway Bias. Includes a 15% hysteresis to prevent route flapping.
- **Zero-Overhead ETX**: Tracks link quality (Expected Transmission Count) without extra probe packets by monitoring sequence-number gaps in standard mesh traffic.
- **Gateway Load Balancing**: Actively distributes traffic across multiple gateways by tracking packets-per-minute (PPM) load metrics.
- **Reliability & Watchdog**: Integrated 30-second task watchdog and heap monitoring (<15KB warning) to ensure long-term stability in remote deployments.
- **Native OTA**: ESP-IDF native Over-The-Air updates with dual-partition support and automatic rollback on boot failure.

---

## Architecture Overview

xMESH follows a strictly modular design, separating core routing logic from hardware-specific implementations.

```text
+-----------------------+      +-----------------------+
|   Application Layer   |      |      xmesh-hal        |
| (firmware/production) |----->| (Display, Sensors)    |
+-----------+-----------+      +-----------------------+
            |
            v
+-----------------------+      +-----------------------+
|      xmesh-core       |      |      xmesh-ota        |
| (Trickle, Cost, ETX)  |<---->| (ESP-IDF Native OTA)  |
+-----------+-----------+      +-----------------------+
            |
            v
+-----------------------+
|      LoRaMesher       |
| (Physical/MAC Layer)  |
+-----------------------+
```

For more details, see the [Architecture Documentation](docs/ARCHITECTURE.md).

---

## Over-The-Air (OTA) Updates

xMESH supports WiFi-based OTA updates for gateway nodes using the ArduinoOTA protocol. This allows for remote firmware maintenance without physical access.

### Safety Mechanisms
- **Dual Partitioning**: Firmware is stored in two slots (`app0`, `app1`).
- **Boot Counter**: A persistent counter in NVS tracks consecutive boot failures.
- **Automatic Rollback**: If the new firmware fails to boot 3 times, the system automatically reverts to the previous known-good version.

Detailed instructions can be found in the [Deployment Guide](docs/DEPLOYMENT.md).

---

## Repository Structure

- `lib/xmesh-core/`: Implementation of the Protocol 3 routing stack (Trickle, Cost, ETX).
- `lib/xmesh-hal/`: Hardware Abstraction Layer for Heltec V3 peripherals (OLED, PMS7003).
- `lib/xmesh-ota/`: Remote update service and version control.
- `firmware/production/`: Reference production firmware implementation (~150 LOC).
- `docs/`: Comprehensive system documentation.

---

## Documentation

- [Architecture Guide](docs/ARCHITECTURE.md): Deep dive into algorithms and data flows.
- [Deployment Guide](docs/DEPLOYMENT.md): Hardware setup and flashing instructions.
- [Changelog](CHANGELOG.md): Version history and release notes.
- [Stability Test Plan](.sisyphus/evidence/stability-test.md): Validation procedures for production.

---

## Project Background

xMESH is a production-focused refactor of "Protocol 3", a research-validated LoRa mesh stack. It transitions from a monolithic research prototype (~2100 LOC) to a clean, modular library architecture (~150 LOC main) suitable for industrial and environmental monitoring applications.

---

## Contributing

We welcome contributions! Please follow these steps:
1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/amazing-feature`).
3. Commit your changes.
4. Push to the branch.
5. Open a Pull Request.

*Note: Please ensure all code follows the naming conventions established in AGENTS.md.*

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details (Placeholder).

---

© 2026 xMESH Team. Optimized for reliable, large-scale LoRa deployments.
