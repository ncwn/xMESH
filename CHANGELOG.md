# Changelog
All notable changes to the xMESH LoRa Mesh Network Research Project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to an 8-week implementation timeline.

## [Week 1] - 2025-10-10

### Added
- Project folder structure:
  - `firmware/{1_flooding, 2_hopcount, 3_gateway_routing, common}/`
  - `raspberry_pi/`
  - `analysis/`
  - `experiments/{configs, results}/`
  - `docs/diagrams/`
- `.gitignore` to exclude proposal documents, AI files, OS files, and build artifacts
- `AI_HANDOFF.md` for multi-AI context management
- `firmware/common/heltec_v3_config.h` with complete pin definitions and LoRa configuration for Heltec WiFi LoRa32 V3 board
- `CHANGELOG.md` to track weekly progress
- **Flooding baseline firmware** (`firmware/1_flooding/`):
  - `platformio.ini` with 3 build environments (sensor, router, gateway)
  - `src/main.cpp` with flooding protocol implementation
  - `src/display.h` and `src/display.cpp` for OLED status display
  - `README.md` with build instructions and protocol documentation
  - Duplicate detection using 5-entry circular cache
  - Role-based behavior (sensors generate packets, routers forward, gateways receive)

### Configuration
- **Hardware:** 5x Heltec WiFi LoRa32 V3 (ESP32-S3 + SX1262)
- **LoRa Settings:** 923.2 MHz (AS923 Thailand), SF7, BW125, 14 dBm, 1% duty cycle
- **Topologies:** A (3-node), B (4-node), C (5-node dual gateway), D (3-node + interference)
- **Build System:** PlatformIO with 3 build configurations per protocol (sensor, router, gateway)
- **Packet Interval:** 60 seconds (sensors)
- **Display Format:** Line1=ID+Role, Line2=TX/RX counts, Line3=Protocol, Line4=Duty cycle

### Technical Details
- Flooding protocol broadcasts all packets to all neighbors
- Duplicate detection prevents infinite loops using (srcAddr, seqNum) cache
- SensorData packet structure includes: seqNum, srcAddr, timestamp, sensorValue, hopCount
- All 3 build environments compile successfully (tested on macOS with PlatformIO)

### Known Issues
- Compiler warnings about `NODE_ROLE_STR` and `ROLE_GATEWAY` redefinition (harmless, can be ignored)
- Cause: LoRaMesher library's internal role definitions conflict with compile flags
- Impact: None - warnings only, functionality not affected

### Notes
- Proposal documents excluded from git repository (reference only)
- Using LoRaMesher library v0.0.10 as base
- Single codebase per protocol with compile-time role configuration
- Display integration adapted from LoRaMesher CounterAndDisplay example

## [Upcoming - Week 2]

### Planned
- Adapt LoRaMesher CounterAndDisplay example for Heltec V3
- Implement flooding baseline firmware
- Create display helper files
- Test compilation and flash to first Heltec board
- Verify display shows Node ID, role, and packet counts

## [Upcoming - Week 3]

### Planned
- Implement hop-count baseline (adapt LoRaMesher Counter example)
- Add RSSI/SNR logging
- Setup Raspberry Pi gateway collector script
- Run first experiments (Topology A: flooding vs hop-count)

## [Upcoming - Week 4-5]

### Planned
- Implement gateway-aware cost routing
- Add cost calculation (RSSI, SNR, ETX, gateway bias)
- Implement hysteresis logic
- Test with Topology A and B

## [Upcoming - Week 6]

### Planned
- Setup dual Raspberry Pi for Topology C
- Test all 4 topologies
- Collect complete experimental dataset (36 experiments)

## [Upcoming - Week 7-8]

### Planned
- Statistical analysis (PDR, latency, overhead)
- Generate plots and graphs
- Write final internship report
- Clean and document code

---

## Legend
- **Added**: New features or files
- **Changed**: Changes to existing functionality
- **Fixed**: Bug fixes
- **Removed**: Removed features or files
- **Configuration**: Hardware or software configuration changes
- **Notes**: Important context or decisions
