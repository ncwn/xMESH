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

### Fixed
- **OLED Display Not Working:** Added Vext pin (GPIO 36) power control - must be set LOW to power OLED on Heltec V3
- **I2C Initialization:** Added explicit Wire.begin(SDA_PIN, SCL_PIN) before display init
- **Role Naming Conflict:** Renamed all role flags from `ROLE_*` to `XMESH_ROLE_*` to avoid collision with LoRaMesher's internal `ROLE_GATEWAY` definition
- **Role Detection Logic:** Fixed `#ifdef` chain in `heltec_v3_config.h` to use `#elif` to prevent multiple role definitions

### Tested
- **Hardware Testing Results:** ✅ All 3 node types verified working on physical Heltec boards
  - **Sensor Node (ID: 6674):** TX=4, RX=2 - Correctly generating packets and receiving rebroadcasts
  - **Router Node (ID: D218):** TX=2, RX=2 - Correctly forwarding all received packets (1:1 ratio)
  - **Gateway Node:** TX=0, RX=2 - Correctly receiving packets without rebroadcasting
- **Duplicate Detection:** Working correctly - duplicate messages appear when same packet received multiple times
- **Display Status:** All nodes showing correct role indicators [S], [R], [G] with accurate TX/RX counters
- **Network Communication:** End-to-end packet flow verified - sensor → router → gateway topology operational

### Known Issues (Resolved)
- ~~Compiler warnings about `NODE_ROLE_STR` and `ROLE_GATEWAY` redefinition~~ ✅ FIXED with `XMESH_ROLE_*` prefix
- ~~OLED display not powering on~~ ✅ FIXED with Vext GPIO 36 control
- ~~Router not rebroadcasting packets~~ ✅ FIXED with role naming conflict resolution

### Notes
- Proposal documents excluded from git repository (reference only)
- Using LoRaMesher library v0.0.10 as base
- Single codebase per protocol with compile-time role configuration
- **Flooding baseline is now HARDWARE TESTED and OPERATIONAL** 🎉
- Display integration adapted from LoRaMesher CounterAndDisplay example

## [Week 2-3] - 2025-10-10

### Added
- **Hop-count routing baseline firmware** (`firmware/2_hopcount/`):
  - `platformio.ini` with 3 build environments (sensor, router, gateway)
  - `src/main.cpp` using LoRaMesher's built-in hop-count routing
  - `src/display.h` and `src/display.cpp` (copied from flooding baseline)
  - `README.md` with routing protocol documentation and comparison with flooding
  - Automatic routing table maintenance via HELLO packets
  - Shortest path selection based on hop count

### Technical Details
- Hop-count routing uses LoRaMesher's internal routing protocol (no manual forwarding)
- Routing tables built automatically through periodic HELLO packet exchanges
- Packets forwarded to specific next hop (not broadcast like flooding)
- Lower network overhead compared to flooding (intelligent routing vs broadcast storm)
- Display shows "HOP-CNT" protocol indicator

### Tested
- **Hardware Testing Results:** ✅ All 3 node types verified working on physical Heltec boards
  - **Sensor Node (ID: BB94):** TX=4, RX=0 - Generating packets every 60s
  - **Router Node (ID: 6674):** TX=0, RX=4 - Receiving packets but not forwarding (direct path available)
  - **Gateway Node (ID: D218):** TX=0, RX=4 - Receiving packets directly from sensor
- **Routing Behavior:** LoRaMesher correctly selected shortest path (direct sensor→gateway, 0 hops)
- **Display Status:** All nodes showing correct role indicators [S], [R], [G] with "HOP-CNT" protocol
- **Network Communication:** End-to-end delivery verified, routing table functioning correctly

### Observations
- Indoor LoRa range excellent (~10-20m through walls easily)
- Router not utilized in current setup due to direct sensor-gateway link being optimal
- LoRaMesher's hop-count routing working as designed (chooses shortest path)
- For controlled topology testing (Week 6), will need physical separation or reduced TX power

### Comparison: Flooding vs Hop-Count
| Metric | Flooding (Week 1) | Hop-Count (Week 2-3) |
|--------|-------------------|----------------------|
| Router TX (same topology) | 2 packets | 0 packets |
| Network Overhead | High (broadcast) | Low (targeted) |
| Routing | Stateless | Stateful (routing table) |
| Path Selection | None (flood all) | Shortest path (hop count) |

### Notes
- Hop-count baseline is **HARDWARE TESTED and OPERATIONAL** ✅
- Protocol successfully demonstrates intelligent routing vs flooding
- Formal topology experiments planned for Week 6
- Using LoRaMesher library v0.0.10 routing features

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
