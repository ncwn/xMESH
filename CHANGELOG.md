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

## [Week 2-3] - 2025-10-11

### Added
- **Hop-count routing baseline firmware** (`firmware/2_hopcount/`):
  - `platformio.ini` with 3 build environments (sensor, router, gateway)
  - `src/main.cpp` using LoRaMesher's built-in hop-count routing protocol
  - `src/display.h` and `src/display.cpp` (adapted from flooding baseline)
  - `README.md` with routing protocol documentation and comparison with flooding
  - Automatic routing table maintenance via HELLO packets (120-second intervals)
  - Shortest path selection based on hop count metric
  - Gateway role advertisement using `ROLE_GATEWAY` flag
  - Routing table debug output (prints every 30 seconds)

### Technical Details
- **Routing Protocol:** LoRaMesher's built-in distance-vector hop-count routing
- **Route Discovery:** Automatic via periodic HELLO packets containing (address, hop count, role)
- **Routing Table:** Each node maintains (destination, next_hop, metric, role, timeout)
- **Route Selection:** Shortest path (minimum hop count) to each destination
- **Gateway Discovery:** Sensors query routing table for closest ROLE_GATEWAY node
- **Packet Forwarding:** LoRaMesher automatically forwards packets based on routing table (no manual code needed)
- **Route Timeout:** 600 seconds (5x HELLO interval) for stale route expiration
- **Display Format:** "HOP-CNT" protocol indicator, shows TX/RX counters

### Configuration Changes
- **Heltec V3 SPI Fix:** Added custom SPI configuration (MOSI=10, MISO=11, SCK=9, required for proper radio communication)
- **LoRa Parameters:** freq=915 MHz, bw=125 kHz, sf=7, cr=4/7, syncWord=0x12, power=14 dBm (default)
- **TX Power Testing:** Attempted -3 dBm minimum power to force multi-hop (sensor and gateway only)
- **Debug Output:** Added routing table printing to observe neighbor discovery and metrics

### Tested
- **Hardware Testing Results:** ✅ All 3 node types verified working on physical Heltec boards
  - **Sensor Node (ID: BB94, kitchen):** TX=5, RX=0 - Generating packets every 60s, finding gateway in routing table
  - **Router Node (ID: 6674, Mac):** TX=0, RX=0 - Ready to forward but not needed (direct path available)
  - **Gateway Node (ID: D218, outside room):** TX=0, RX=5 - Receiving packets directly from sensor
- **Routing Table Validation:** ✅ All nodes correctly discovering neighbors via HELLO packets
  - Router sees: BB94 (sensor, 1 hop), D218 (gateway, 1 hop, ROLE_GATEWAY=0x01)
  - Sensor sees: D218 (gateway, 1 hop, ROLE_GATEWAY=0x01), 6674 (router, 2 hops via gateway)
- **Gateway Role Propagation:** ✅ ROLE_GATEWAY flag correctly set and propagated via HELLO packets
- **Route Selection:** ✅ Sensor correctly queries routing table and selects gateway with shortest path
- **Network Communication:** ✅ End-to-end delivery working, packets reach gateway successfully
- **Display Status:** ✅ All nodes showing correct role indicators [S], [R], [G] with "HOP-CNT" protocol

### Fixed
- **SPI Communication Issue:** Added custom SPI initialization (`customSPI.begin()`) required for Heltec V3 hardware
- **LoRa Parameters:** Set explicit freq, bandwidth, SF, CR parameters matching working Heltec examples
- **Routing Table Query:** Fixed sensor code to query `radio.getClosestGateway()` instead of broadcasting
- **Gateway Role:** Added `radio.addGatewayRole()` on gateway nodes for proper role advertisement

### Observations
- **LoRa Range:** Exceptional indoor range even at minimum power (-3 dBm)
  - Direct communication maintained across 20-30 meters through walls
  - Sensor and gateway can communicate directly despite physical separation (kitchen → outside room)
  - Even with minimum TX power, all nodes within range of each other in typical home environment
- **Routing Behavior:** LoRaMesher correctly selecting optimal (direct) path
  - Router not utilized in current setup because direct sensor→gateway path is shortest (1 hop vs 2 hops)
  - This is **correct intelligent routing behavior** - mesh chooses best available path
  - Router confirmed functional through routing table analysis (all nodes see it)
- **Multi-hop Challenge:** Difficult to force multi-hop routing in constrained test environment
  - Would require >50 meter separation or RF shielding (Faraday cage)
  - For controlled topology experiments (Week 6), will use larger spaces or physical barriers

### Comparison: Flooding vs Hop-Count
| Metric | Flooding (Week 1) | Hop-Count (Week 2-3) |
|--------|-------------------|----------------------|
| **Router TX (same topology)** | 2 packets | 0 packets (direct path used) |
| **Network Overhead** | High (broadcast storm) | Low (targeted forwarding) |
| **Routing** | Stateless (no memory) | Stateful (routing table) |
| **Path Selection** | None (flood to all) | Shortest path (hop count metric) |
| **Duplicate Detection** | Manual (5-entry cache) | Automatic (LoRaMesher) |
| **Scalability** | Poor (O(n²) messages) | Better (O(n) with routing) |
| **Route Discovery** | None | Automatic (HELLO packets) |
| **Packet Addressing** | Broadcast to all | Unicast to next hop |
| **Memory Usage** | Low (stateless) | Higher (routing table) |

### Research Findings
1. **Intelligent Routing Works:** LoRaMesher's hop-count routing successfully selects shortest available path
2. **LoRa Range Advantage:** Sub-GHz LoRa provides excellent penetration even at minimum power
3. **Topology Control Important:** For multi-hop testing, need controlled RF environments or larger spaces
4. **Mesh is Self-Organizing:** All nodes automatically discover neighbors and build routing tables
5. **Gateway Discovery Functional:** Role-based routing allows sensors to find gateways dynamically

### Notes
- Hop-count baseline is **HARDWARE TESTED and OPERATIONAL** ✅
- Protocol successfully demonstrates intelligent routing vs flooding
- Routing tables validated through debug output - all nodes discovering neighbors correctly
- For Week 6 topology experiments, will use larger building spaces or RF barriers
- Using LoRaMesher library v0.0.10 built-in routing features
- Heltec V3 custom SPI configuration critical for proper operation

### Git
- Commit: "Week 2-3: Hop-count routing baseline with intelligent path selection"
- Tag: v0.3.1-alpha



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
