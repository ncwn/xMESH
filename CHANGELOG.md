# Changelog

All notable changes to the xMESH project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-01-29

### Added
- **Modular Library Architecture**: Split research prototype into `xmesh-core`, `xmesh-hal`, and `xmesh-ota`.
- **Advanced Routing Stack**:
    - **Trickle Scheduler**: RFC 6206 adaptive HELLO scheduling to reduce control overhead.
    - **Cost Router**: Multi-metric path selection (RSSI, SNR, ETX, Hops, Gateway Bias).
    - **ETX Tracker**: Zero-overhead Link Quality Estimation via sequence-gap detection.
    - **Gateway Balancer**: Active load distribution for multi-gateway deployments.
- **Production-Ready OTA**:
    - WiFi-based updates via ArduinoOTA integration.
    - ESP-IDF native dual-partition management (`app0`/`app1`).
    - NVS-based boot failure tracking with automatic rollback (3 consecutive failures).
- **Hardware Abstraction**:
    - OLED display driver for Heltec V3.
    - Sensor interfaces for environmental monitoring.
- **Reliability Features**:
    - 30-second Task Watchdog (WDT) protection.
    - 15KB heap monitoring and proactive logging.
    - Comprehensive error handling with >19 ESP_LOG levels.
- **Reference Implementation**: `firmware/production/` provided as a clean, minimal (~150 LOC) base for field deployment.

### Changed
- Refactored monolithic research code (~2100 LOC) into modular libraries.
- Standardized build system on PlatformIO.
- Improved LoRaMesher compatibility via optional callback hooks.

### Removed
- Removed hardcoded configuration from source; moved to `config.h` and `platformio.ini`.
- Deprecated legacy "Protocol 1" and "Protocol 2" research components in favor of "Protocol 3" (xMESH).

### Note on Research History
xMESH is the production evolution of the "Protocol 3" research prototype. For historical context, raw experimental data, and legacy implementations, refer to the `research` branch or the `.sisyphus/research-snapshot/` directory (if available).
