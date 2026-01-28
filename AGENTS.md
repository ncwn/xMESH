# xMESH: Production LoRa Mesh Documentation (AI-Optimized)

## Project Overview
xMESH is a professional-grade LoRa mesh network designed for production IoT deployments. It is a modular refactor of a research prototype ("Protocol 3") that focuses on scalability, reliability, and efficient routing in resource-constrained environments.

### Core Objectives
- **Scalability**: Sublinear control overhead growth via adaptive scheduling.
- **Reliability**: Multi-metric cost routing and zero-overhead ETX tracking.
- **Production-Ready**: Clean separation of concerns (Core, HAL, OTA) and minimized main firmware logic.

## Architecture
The project is structured into modular libraries to ensure hardware portability and algorithmic stability.

- **`lib/xmesh-core/`**: Implementation of the Protocol 3 routing stack.
  - `CostRouter`: Multi-metric path selection (RSSI, SNR, ETX, Hops, Gateway Bias).
  - `TrickleScheduler`: RFC 6206-inspired adaptive HELLO scheduling.
  - `ETXTracker`: Zero-overhead Link Quality Estimation (LQE) via sequence-gap detection.
  - `GatewayBalancer`: Active load sharing across multiple network gateways.
- **`lib/xmesh-hal/`**: Hardware Abstraction Layer for Heltec V3.
  - SSD1306 OLED display drivers.
  - Environmental sensor interfaces.
- **`lib/xmesh-ota/`**: ESP-IDF native Over-The-Air update integration.
- **`firmware/production/`**: Reference implementation (~200 LOC) for field deployment.

## Build Commands
All builds use PlatformIO (PIO).

| Task | Command |
|------|---------|
| Build Firmware | `pio run` |
| Upload to Device | `pio run -t upload` |
| Serial Monitor | `pio device monitor` |
| Clean Build | `pio run -t clean` |

## Key Conventions
1. **Algorithm Integrity**: Do NOT tune or modify the core routing algorithms (Trickle, Cost Function) without explicit instruction. These are validated against research baselines.
2. **Modular HAL**: Hardware-specific logic belongs in `xmesh-hal`. `xmesh-core` must remain hardware-agnostic.
3. **No Interactive Input**: All configuration should be handled via `platformio.ini` build flags or header constants.
4. **Error Handling**: Use the built-in logging system; avoid raw `printf` in library code.

## Hardware Specification
- **MCU**: ESP32-S3 (Heltec WiFi LoRa 32 V3)
- **Radio**: SX1262 LoRa
- **Display**: 0.96" OLED (SSD1306)
- **Frequency**: Region-specific (default AS923/EU868)
