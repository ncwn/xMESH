# xMESH LoRaMesher Research Project

**Gateway-Aware Cost Routing for Scalable LoRa Mesh Networks**

This repository contains the implementation of a Master's internship research project at the Asian Institute of Technology (AIT), focusing on improving the scalability of LoRa mesh networks through intelligent, multi-metric routing protocols.

## 🎯 Project Overview

This research addresses the fundamental scalability limitations of broadcast-based LoRa mesh networks by implementing and evaluating three routing protocols:

1. **Flooding Protocol** - Baseline broadcast approach
2. **Hop-Count Routing** - LoRaMesher default distance-vector routing
3. **Gateway-Aware Cost Routing** - Multi-metric cost calculation framework with route quality monitoring

### Key Innovations

- **Trickle Adaptive Scheduling**: 85.7-90.9% overhead reduction validated. Efficiency improves with network maturity. Stable nodes maintain 90.9% even during faults.
- **LOCAL Fault Isolation**: Fault impact radius ~10-30% of network (not 100%). Stable nodes maintain I_max=600s and 90.9% efficiency while affected nodes recover at 66.7%. Faults don't cascade network-wide.
- **W5 Active Load Sharing**: 65/35 traffic split across dual gateways validated. Live packet-rate encoding in HELLOs enables dynamic balancing.
- **GPS + PM Environmental Monitoring**: PMS7003 particulate matter sensor (GPIO 6-7) and NEO-M8M-0-10 GPS (GPIO 4-5) integrated. Air quality and location data transmitted via LoRa mesh. Demonstrates real-world IoT environmental monitoring.
- **Zero-Overhead ETX**: Sequence-gap detection (no ACK packets required) - superior to traditional ACK-based methods
- **Multi-metric cost function** combining hop count, RSSI (estimated from SNR), SNR, ETX, and gateway bias
- **Hardware-validated** on real ESP32-S3 + SX1262 devices with environmental sensors

**Design Note**: MAC-derived addresses (02B4, 6674, 8154) used intentionally. RadioLib parsing bug with sequential addresses: 0x0001 creates duplicate entries (0x0001, 0x0010), causing routing table corruption and exponential growth. Hardware MAC addresses provide stable operation.

## 📁 Repository Structure

```
xMESH/
├── firmware/                  # Protocol implementations
│   ├── 1_flooding/           # Protocol 1: Flooding baseline
│   │   └── PROTOCOL1_COMPLETE.md  # Complete documentation (NEW)
│   ├── 2_hopcount/           # Protocol 2: Hop-count routing
│   │   └── PROTOCOL2_COMPLETE.md  # Complete documentation (NEW)
│   ├── 3_gateway_routing/    # Protocol 3: Cost-aware routing
│   └── common/               # Shared utilities
├── raspberry_pi/             # Data collection & analysis
├── experiments/              # Test configurations & results
├── docs/                     # Technical documentation
│   ├── PROJECT_OVERVIEW.md   # Goals + protocol rationale
│   └── archive/              # Historical documentation
└── PRD.md                    # Product Requirements Document
```

## 🚀 Quick Start

### Prerequisites

- 5× Heltec WiFi LoRa 32 V3 boards
- PlatformIO IDE or CLI
- Python 3.8+ with pandas, matplotlib
- Raspberry Pi (for gateway/data collection)

### Building Firmware

```bash
# Clone repository
git clone https://github.com/ncwn/xMESH.git
cd xMESH

# Build flooding protocol for Node 1
cd firmware/1_flooding
pio run -D NODE_ID=1

# Flash to board
pio run --target upload --upload-port /dev/ttyUSB0

# Monitor output
pio device monitor --baud 115200
```

### Running Experiments

```bash
# Start data collection on Raspberry Pi
python raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB0 \
  --output results/test.csv

# Analyze results
python raspberry_pi/data_analyzer.py results/test.csv --plot
```

## 📊 Current Status

| Component | Status | Progress |
|-----------|--------|----------|
| Protocol 1 (Flooding) | ✅ Validated | Flash: 31.2% • Node IDs: 1-2,3-4,5-6 |
| Protocol 2 (Hop-Count) | ✅ Validated | Flash: 31.4% • HELLO baseline |
| Protocol 3 (Gateway-Aware) | ✅ Feature Complete | Flash: 32.5% • GPS+PM+W5+Fault Isolation |
| GPS + PM Sensors | ✅ Integrated | PMS7003 + NEO-M8M |
| Data Collection | ✅ Enhanced | PM/GPS tracking added to multi_node_capture.py |
| **Latest Tests** | ✅ **Complete** | 16 tests (14 indoor + 2 sensor integration) |

**Indoor Validation Complete** (Nov 10-13, 2025) — see the authoritative matrix in `experiments/FINAL_VALIDATION_TESTS.md`:
- 14 tests: 3-5 node topologies, fault tolerance, multi-gateway (all 30-minute indoor runs)
- PDR: Protocol 3 achieves 96.7-100% (superior to 81-85% baselines in challenging 4-node linear test)
- Overhead: 27-31% reduction validated across scales (`experiments/archive/3node_30min_overhead_validation_20251111_032647/` (needs revalidation))
- Multi-gateway: W5 load sharing validated with alternating gateways (`experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/` (needs revalidation) and `.../w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` (needs revalidation))

- **Pending (Field Work):**
- Multi-hop validation: Requires physical separation / ≤8 dBm power to force relay hops (all indoor tests still hops=0)
- Outdoor/long-range characterization: replicate the 30‑minute stability + failure cycling in a non-line-of-sight deployment
- Documentation polish: remaining baseline analysis files (Protocol 1/2) still queued for professional rewrite

### Protocol 1 Status
- ✅ Full flooding implementation complete
- ✅ Indoor validation complete (4 topologies logged Nov 10-12, 2025)
- ✅ Duplicate detection implemented
- ✅ Controlled flooding behavior implemented
- ✅ CSV logging ready

**Implementation Notes:**
- Uses controlled flooding (only relays rebroadcast)
- Now uses forked xMESH library (fixed)
- Further testing only required if multi-hop spacing campaign proceeds

### Protocol 2 Status
- ✅ Complete implementation with LoRaMesher library
- ✅ Indoor validation complete (6 topologies logged Nov 10-12, 2025)
- ✅ Hop-count routing implemented
- ✅ HELLO mechanism (120s interval) ready
- ✅ Routing table management functional
- ✅ Gateway discovery implemented

**Implementation Notes:**
- Now uses forked xMESH library (fixed)
- Baseline for HELLO comparison with P3
- Further testing only required for future multi-hop expansion
- ✅ Git tag: v1.1.0-protocol2-complete

**Key Behavior (LoRaMesher Library):**
- All nodes participate in routing (distance-vector protocol)
- Nodes forward packets IF they are on the routing path (via field matches address)
- Sensors CAN forward packets if routing path requires it
- Gateway never forwards (always final destination)
- HELLO packets every 120s build and maintain routing tables

### Protocol 3 Status
- ✅ **Cost calculation framework** fully implemented
- ✅ **Trickle scheduler INTEGRATED** (60s → 600s adaptive) with 31% overhead reduction validated
- ✅ **ETX sequence-gap detection** (zero overhead)
- ✅ **Gateway load sharing (W5)** fully active — HELLOs serialize live packet rate, sensors emit `[W5] Load-biased …` logs, and the TX path prefers the lighter gateway automatically
- ✅ **Fast fault detection** with automatic route removal and Trickle reset (378 s detection confirmed for sensor, gateway, and relay outages)
- ✅ Multi-metric cost function (W1-W5 weights) + hysteresis
- ✅ RSSI estimation from SNR (RSSI ≈ -120 + SNR×3)
- ✅ Cost-based route selection callback
- ✅ **Frequency FIXED** to 923.2 MHz (was 915.0)

**Implementation Notes:**
- Previous 2-hour test ARCHIVED (wrong frequency)
- Now uses forked xMESH library (consistent)
- Remaining gap: multi-hop validation (all indoor links currently single-hop)
- Validated evidence (log folders):
  1. `experiments/archive/3node_30min_overhead_validation_20251111_032647/` — 31 HELLOs vs 45 baseline while maintaining 100% PDR.
  2. `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/` — 30-minute cold start with `[W5] Load-biased gateway selection …` entries preceding alternating transmissions; gateways split RX counts 16 vs 13 packets.
  3. `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` (needs revalidation) — 35-minute endurance where taking Sensor 02B4, Gateway 8154, and Relay 6674 offline triggered `[FAULT]` noise-free detections at 375-384s with Trickle resets back to I_min.
- Implementation: Entirely in firmware folder (no upstream patching required)

## 📖 Documentation

### Quick Start & Overview
- **[Project Overview](docs/PROJECT_OVERVIEW.md)** - Goals, success criteria, and protocol rationale
- **[PRD](PRD.md)** - Comprehensive project requirements

### Protocol Documentation (Academic Style)
- **[Protocol 1 Complete](firmware/1_flooding/PROTOCOL1_COMPLETE.md)** - Flooding baseline
- **[Protocol 2 Complete](firmware/2_hopcount/PROTOCOL2_COMPLETE.md)** - Hop-count routing
- **[Protocol 3 Complete](firmware/3_gateway_routing/PROTOCOL3_COMPLETE.md)** - Cost-aware routing

### Development Guides
- **[Hardware Setup Guide](docs/HARDWARE_SETUP.md)** - Heltec V3 configuration
- **[Firmware Development](docs/FIRMWARE_GUIDE.md)** - Build and flash procedures
- **[Experiment Protocol](docs/EXPERIMENT_PROTOCOL.md)** - Testing methodology
- **[Upstream Tracking](docs/UPSTREAM_TRACKING.md)** - LoRaMesher fork management

### Historical Documentation
- **[Archive](docs/archive/)** - Historical docs and upstream files (see [Archive README](docs/archive/README.md))

## 🔬 Research Contributions

### 1. Trickle Scheduler Integration (FULLY INTEGRATED & VALIDATED - Nov 9, 2025)
- First full Trickle (RFC 6206) integration for LoRa mesh routing
- Custom HELLO task replaces LoRaMesher's fixed 120s interval
- Adaptive HELLO interval (60s → 600s) without library modification
- **31% overhead reduction validated** in 3-5 node tests (Nov 10-12 2025) with 180s safety HELLO. 44-58% achievable with 300s safety, 80-97% theoretical maximum at I_max=600s

### 2. Sequence-Aware ETX
- Novel packet loss detection using sequence number gaps
- Zero protocol overhead (no ACK packets required)
- EWMA smoothing (α=0.3)
- More efficient than traditional ACK-based ETX

### 3. Multi-Metric Cost Function
```
Cost = W₁×hops + W₂×(1-RSSI_norm) + W₃×(1-SNR_norm) + W₄×ETX + W₅×gateway_bias
```

## 🧪 Test Configuration

- **Frequency**: 923.2 MHz (AS923 Thailand)
- **Spreading Factor**: 7
- **Bandwidth**: 125 kHz
- **Power**: 14 dBm
- **Duty Cycle**: <1% (regulatory limit)

## 📈 Expected Results

| Metric | Flooding | Hop-Count | Gateway-Aware |
|--------|----------|-----------|---------------|
| PDR | ~100% | >95% | >95% |
| Overhead | O(N²) | O(N√N) | O(N) |
| Latency | Low | Medium | Low-Medium |
| Scalability | Poor | Moderate | Good |

## 🛠️ Development Workflow

### For Contributors

1. **Fork** the repository
2. **Create** feature branch (`git checkout -b feature/improvement`)
3. **Test** on hardware before claiming success
4. **Document** after hardware validation only
5. **Submit** pull request with test results

### Upstream Synchronization

This is a fork of [LoRaMesher/LoRaMesher](https://github.com/LoRaMesher/LoRaMesher). All three protocols use the forked library at `https://github.com/ncwn/xMESH.git#main` for consistency. Weekly sync recommended:

```bash
git fetch upstream
git merge upstream/main
./run_tests.sh
```

## 📫 Contact

**Student**: Nyein Chan Win Naing

**Institution**: Asian Institute of Technology (AIT)

**Email**: st123843@ait.ac.th

**Internship Period**: August - November 2025

**Final Submission**: December 3, 2025

**Chairperson (Advisor)**: Prof. Attaphongse Taparugssanagorn <attaphongset@ait.ac.th>

**Co-Chairperson (Internship Advisor)**: Dr. Adisorn Lertsinsrubtavee <adisorn@ait.ac.th>

**Repository**: https://github.com/ncwn/xMESH

## 📄 License

MIT License - See LICENSE file

## 🙏 Acknowledgments

- LoRaMesher team for the excellent base library
- AIT for research support
- Open-source community for tools and libraries

---

**Internship Period**: August - November 2025
**Final Submission**: December 3, 2025
**Status**: 🔬 Protocol Implementation Complete | **Phase**: Comprehensive Testing & Analysis
