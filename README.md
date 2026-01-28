# xMESH: Production LoRa Mesh Network

xMESH is a professional-grade LoRa mesh network designed for production IoT deployments. It provides a modular, reliable, and scalable routing stack optimized for ESP32-S3 hardware.

## Quick Start

### 1. Requirements
- **Hardware**: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
- **Software**: PlatformIO (PIO) Core or Home

### 2. Build & Flash
```bash
# Clone the repository
git clone <repository-url>
cd xMESH

# Build the production firmware
pio run

# Upload to your Heltec V3 device
pio run -t upload

# Monitor serial output
pio device monitor
```

## Key Features

- **Adaptive Scheduling (Trickle)**: Reduces control traffic by up to 40% in stable networks using RFC 6206-inspired algorithms.
- **Multi-Metric Cost Routing**: Optimizes paths based on RSSI, SNR, ETX, Hop Count, and Gateway Bias.
- **Zero-Overhead ETX**: Tracks link quality without extra probe packets via sequence-gap detection.
- **Gateway Load Balancing**: Actively shares traffic across multiple gateways to prevent bottlenecks.
- **OTA Updates**: Native ESP-IDF Over-The-Air update integration for remote maintenance.

## Repository Structure

- `lib/xmesh-core/`: Core routing logic (Trickle, Cost Function, ETX).
- `lib/xmesh-hal/`: Hardware abstraction for Heltec V3 (OLED, Sensors).
- `lib/xmesh-ota/`: OTA update service.
- `firmware/production/`: Reference production firmware implementation.

## Project Background
xMESH is a production-focused refactor of "Protocol 3", a research-validated LoRa mesh stack. It transitions from a monolithic research prototype to a clean, modular library architecture suitable for industrial and environmental monitoring applications.

## License
[Insert License Here - e.g., MIT or Apache 2.0]
