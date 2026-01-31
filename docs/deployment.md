# xMESH Deployment Guide

This guide covers the requirements, procedure, and best practices for deploying xMESH in production environments.

## Hardware Requirements

xMESH is optimized for the **Heltec WiFi LoRa 32 V3** development board.

| Component | Specification |
|-----------|---------------|
| **MCU** | ESP32-S3 (Dual-core, WiFi/BT) |
| **LoRa Radio** | SX1262 |
| **OLED Display** | 0.96 inch 128x64 (SSD1306) |
| **Flash** | 4MB (8MB optional) |
| **Frequency** | Region-specific (AS923, EU868, US915) |

### Pre-Deployment Checklist
- [ ] Heltec V3 hardware verified (antenna attached).
- [ ] PlatformIO installed on development machine.
- [ ] Access to WiFi (for Gateway nodes and OTA updates).
- [ ] Correct LoRa frequency region set in `platformio.ini`.

## Flashing Procedure

### 1. Build Firmware
Compile the production firmware using PlatformIO:
```bash
cd firmware/production
pio run
```

### 2. Upload via USB
Connect your Heltec V3 via USB and run:
```bash
pio run -t upload
```

### 3. Verify Serial Output
Monitor the initialization sequence:
```bash
pio device monitor --baud 115200
```
Expected output:
```text
[XMESH] Starting Production Firmware...
[XMESH] Watchdog initialized (30s)
[XMESH] LoRaMesher started
[XMESH] Routing callbacks registered
[XMESH] Free heap: 184520 bytes
```

## Gateway Configuration

A Gateway node requires WiFi connectivity to bridge mesh traffic to the internet or provide OTA updates.

### WiFi Setup
Configure WiFi credentials in `firmware/production/include/config.h`:
```cpp
#define WIFI_SSID "Your_SSID"
#define WIFI_PASSWORD "Your_Password"
```

### OTA Updates
To trigger an OTA update over WiFi:
1. Ensure the node is connected to WiFi and its IP is known.
2. Run the upload command targeting the IP:
```bash
pio run -t upload --upload-port <IP_ADDRESS>
```

## Network Topology Recommendations

### Linear (Chain)
Ideal for long-range coverage along roads or pipelines.
- **Max Hops**: 8-10.
- **Placement**: Nodes should have line-of-sight if possible.

### Star / Cluster
Ideal for dense deployments (e.g., smart agriculture).
- **Gateway**: Centralized for shortest hop counts.
- **Redundancy**: Use multiple gateways for load balancing via `GatewayBalancer`.

### Grid Mesh
Best for urban or industrial environments.
- **Redundancy**: High path diversity.
- **Trickle**: Set $I_{max}$ higher (600s+) to minimize collision in dense grids.

## Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| **No Mesh Formation** | Mismatched Frequency | Check `platformio.ini` build flags. |
| **Frequent Reboots** | Power Supply | Ensure 5V/500mA+ source; LoRa TX peaks. |
| **OTA Rollback** | Unstable Build | Check boot logs; verify `partitions.csv` exists. |
| **High Packet Loss** | Poor ETX | Improve antenna placement; reduce distance. |

### Log Analysis
Use `grep` to filter production logs for issues:
- **Errors**: `grep "ESP_LOGE" logs.txt`
- **Warnings**: `grep "ESP_LOGW" logs.txt`
- **Routing**: `grep "CostRouter" logs.txt`
- **Trickle**: `grep "Trickle" logs.txt`

### Monitoring Tools
- **Serial Monitor**: Real-time debugging via USB.
- **Grafana/Dashboard**: If connected via Gateway, monitor packet delivery ratio (PDR) and heap health.
