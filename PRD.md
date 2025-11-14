# Product Requirements Document: xMESH LoRaMesher Research Project

## Executive Summary

**Project Name**: xMESH - Gateway-Aware Cost Routing for LoRa Mesh Networks
**Type**: Master's Internship Research Project
**Institution**: Asian Institute of Technology (AIT)
**Student**: Nyein Chan Win Naing (st123843@ait.asia)
**Duration**: August - November 2025
**Final Submission**: December 3, 2025
**Chairperson (Advisor)**: Prof. Attaphongse Taparugssanagorn (attaphongset@ait.ac.th)
**Co-Chairperson (Internship Advisor)**: Dr. Adisorn Lertsinsrubtavee (adisorn@ait.ac.th)
**Repository**: https://github.com/ncwn/xMESH

### Project Vision
Design, implement, and empirically evaluate scalable LoRa mesh network protocols that address the fundamental scalability limitations of broadcast-based approaches in Low-Power Wide-Area Networks (LPWANs) through intelligent, gateway-aware cost routing.

## 1. Project Context

### 1.1 Problem Statement
Current LoRa mesh networking solutions suffer from:
- **O(N²) scaling** with broadcast flooding approaches
- **Suboptimal path selection** using only hop count metrics
- **High control overhead** from fixed-interval routing updates
- **Poor link quality awareness** in routing decisions
- **Lack of gateway optimization** for IoT deployments

### 1.2 Research Objectives
1. Establish functional multi-node LoRa mesh testbed using Heltec LoRa 32 V3 boards
2. Implement and compare three routing protocols:
   - **Protocol 1**: Pure flooding baseline
   - **Protocol 2**: LoRaMesher hop-count routing
   - **Protocol 3**: Gateway-aware cost routing with Trickle scheduling
3. Validate scalability improvements through empirical testing
4. Contribute improvements back to open-source community

### 1.3 Success Metrics
- **31% reduction** in control overhead validated (Nov 11 2025, with 180s safety HELLO)
- **>95% PDR** maintained across all protocols ✅ (96.7-100% achieved across 3-5 nodes)
- **40% reduction** in total network traffic vs flooding (pending physical distance multi-hop test)
- **Improved path quality** through multi-metric cost calculation ✅
- **Multi-gateway**: Discovery and cost-based selection validated ✅

## 2. Technical Architecture

### 2.1 Hardware Specifications

| Component | Specification | Quantity |
|-----------|--------------|----------|
| Board | Heltec WiFi LoRa 32 V3 | 5 units |
| MCU | ESP32-S3 (240 MHz dual-core) | - |
| LoRa Chip | Semtech SX1262 | - |
| Display | 128x64 OLED (SSD1306) | - |
| Memory | 512KB SRAM + 8MB PSRAM | - |
| Gateway | Raspberry Pi 4B | 1 unit |

### 2.2 Software Stack

```
┌─────────────────────────────────────┐
│        Application Layer            │
│    (Sensor Data Generation)         │
├─────────────────────────────────────┤
│        Protocol Layer               │
│  (Flooding/HopCount/CostRouting)    │
├─────────────────────────────────────┤
│      LoRaMesher Library             │
│    (Routing & Packet Services)      │
├─────────────────────────────────────┤
│        RadioLib HAL                 │
│      (Hardware Abstraction)         │
├─────────────────────────────────────┤
│      ESP32 Arduino Framework        │
│         (FreeRTOS + HAL)            │
└─────────────────────────────────────┘
```

### 2.3 Project Structure

```
xMESH/
├── firmware/                  # Protocol implementations
│   ├── 1_flooding/           # Baseline 1: Pure flooding
│   ├── 2_hopcount/           # Baseline 2: LoRaMesher default
│   ├── 3_gateway_routing/    # Main: Cost-aware routing
│   └── common/               # Shared utilities
├── raspberry_pi/             # Data collection & analysis
│   ├── serial_collector.py   # Real-time data collection
│   ├── data_analyzer.py      # Statistical analysis
│   └── mqtt_monitor.py       # Live monitoring
├── experiments/              # Test artifacts
│   ├── topologies/           # Network layouts
│   ├── results/              # Raw data
│   └── analysis/             # Processed results
├── docs/                     # Technical documentation
│   ├── HARDWARE_SETUP.md     # Board configuration
│   ├── FIRMWARE_GUIDE.md     # Development guide
│   ├── EXPERIMENT_PROTOCOL.md # Testing procedures
│   └── UPSTREAM_TRACKING.md  # Fork management
└── proposal_docs/            # Research proposal (97 pages)
```

## 3. Implementation Status

### 3.1 Implementation Status (Nov 15, 2025 - GPS+PM Integration Complete)

#### Protocol 1: Flooding (100% Complete)
- Broadcast-based packet dissemination
- Duplicate detection using (src, seq) cache
- TTL-based loop prevention
- Gateway flood termination
- Random rebroadcast delays

#### Protocol 2: Hop-Count Routing (100% Complete) ✅ VALIDATED
- LoRaMesher library integration
- Distance-vector routing with hop count
- Fixed 120s HELLO intervals (LoRaMesher default)
- Unicast forwarding via routing table
- All nodes participate in forwarding (if on routing path)
- Hardware validated: 100% PDR
- Git tag: v1.1.0-protocol2-complete

**LoRaMesher Routing Behavior:**
- Nodes forward packets IF packet.via == node.address (on routing path)
- Nodes drop packets IF packet.via != node.address (not on path)
- Sensors CAN forward if routing topology requires it
- Gateway never forwards (always final destination)
- HELLO packets build routing tables automatically (120s interval)

#### Protocol 3: Gateway-Aware Cost Routing (Code Complete — Multi-Hop Validation Pending)

**✅ Completed Components:**
- **Trickle Scheduler** (RFC 6206-inspired)
  - Adaptive HELLO interval calculation (60s → 600s)
  - k=1 suppression threshold
  - Safety timer (5 minutes)
  - 31% overhead reduction validated in `experiments/archive/3node_30min_overhead_validation_20251111_032647/` (needs revalidation) (31 HELLOs vs 45 baseline). 44–58% achievable with 300 s safety, 80–97% theoretical with no safety timer.

- **ETX Link Quality Tracking**
  - Sequence-aware gap detection
  - Sliding window (10 packets)
  - EWMA smoothing (α=0.3)
  - No additional protocol overhead

- **Multi-Metric Cost Calculation**
  ```
  Cost = W₁×hops + W₂×(1-RSSI_norm) + W₃×(1-SNR_norm) + W₄×ETX + W₅×gateway_bias
  Weights: W₁=1.0, W₂=0.3, W₃=0.2, W₄=0.4, W₅=1.0
  ```

  **Note**: RSSI is estimated from SNR using formula `RSSI ≈ -120 + SNR×3` (not directly measured from RadioLib)

**✅ Completed:**
- Cost metrics calculation and monitoring
- Hysteresis logic for route stability (15% threshold implemented)

**⚠️ Pending Validation (Research Work):**
- Cost-based route selection driven by RSSI/SNR/ETX — requires physical multi-hop topology with alternative relays (hallway/parking-lot test at ≤8 dBm).
- Weight tuning experiments (optional) once multi-hop evidence is captured.

### 3.2 Infrastructure Components

#### Data Collection Pipeline ✅
```python
# Raspberry Pi → SQLite → MQTT → Analysis
serial_collector.py  # Real-time CSV/DB logging
data_analyzer.py     # Statistical processing
mqtt_monitor.py      # Live dashboard
```

#### Display Integration ✅
- Real-time node status
- Link quality metrics
- Packet statistics
- Duty cycle monitoring
- Multi-page interface

#### Duty Cycle Enforcement ✅
- AS923 compliance (1% limit)
- Airtime calculation
- Warning system (83%, 94% thresholds)
- Automatic blocking when exceeded

### 3.3 Validation Snapshot (Nov 15, 2025)

- **Protocol 1 (Flooding)** – 4 indoor runs (3–5 nodes, Nov 10–11) captured 96–100 % PDR. Logs: `experiments/results/protocol1/30min_val_10dBm_20251110_174519/`, `.../4node_linear_*`, `.../4node_diamond_*`.
- **Protocol 2 (Hop-Count)** – 6 indoor runs (3–5 nodes, Nov 10–12) hit 96–100 % PDR and provide the 120 s HELLO baseline (45–60 HELLOs / 30 min). Logs: `experiments/results/protocol2/30min_val_10dBm_20251110_182442/`, `.../4node_linear_*`, `.../5node_linear_*`.
- **Protocol 3 (Gateway-Aware)** – Comprehensive evidence set:
  1. `experiments/archive/3node_30min_overhead_validation_20251111_032647/` ⇒ 31 HELLOs vs 45 baseline (31 % reduction) with 100 % PDR.
  2. `4node_linear_20251111_175245` and `4node_diamond_*` ⇒ 96.7–100 % PDR, outperforming Protocols 1 & 2 in the most lossy scenario.
  3. `5node_linear_stable_20251112_024908` and `..._after-10min-one-sensor-down_20251112_032513` ⇒ 100 % PDR and 27 % HELLO reduction during 5-node runs plus fault injection.
  4. `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/` (30 min cold start) and `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` (35 min endurance) ⇒ sensors log `[W5] Load-biased gateway selection …`, gateways split packet counts (16 vs 13), and `[FAULT]` entries show 375–384 s detection for sensor, gateway, and relay outages.
- **Remaining Gap** – Multi-hop proof (forced hops > 0 via hallway/parking-lot spacing at ≤8 dBm). This is the last open validation task before thesis writing.

## 4. Testing & Validation

### 4.1 Test Topologies

```
Linear (3 nodes):     [1]---10m---[2]---10m---[3/GW]

Diamond (4 nodes):         [2]
                          /    \
                        5m      5m
                       /          \
                     [1]          [4/GW]
                       \          /
                        5m      5m
                          \    /
                           [3]

Mesh (5 nodes):      [1]--10m--[2]
                      | \     / |
                      |   \ /   |
                     10m   X   10m
                      |   / \   |
                      | /     \ |
                     [3]--10m--[4]
                           |
                          10m
                           |
                         [5/GW]
```

### 4.2 Test Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Duration | 60 minutes | Statistical significance |
| Repetitions | 3 per scenario | Confidence intervals |
| Packet Rate | 1/minute | Conservative duty cycle |
| Frequency | 923.2 MHz | AS923 Thailand |
| SF/BW | 7/125kHz | Speed optimization |
| Power | 10 dBm (indoor validation), 6–8 dBm planned for hallway multi-hop | Indoor runs stay well below duty-cycle limit; reduced power reserved for forced multi-hop |

### 4.3 Validation Gates

**Hardware Testing Required Before Documentation (status Nov 13, 2025):**
- [x] All nodes communicate successfully
- [x] OLED displays correct information
- [x] CSV logging functional
- [x] Duty cycle < 1% (gateway logs show ≤0.1% during 30–35 min runs)
- [x] Routes converge within 5 HELLO intervals (≤60 s in logs)
- [x] PDR > 95% (96.7–100% across 3–5 nodes)
- [x] Gateway preference working (W5 bias confirmed by `[W5] Load-biased gateway selection …` logs)

**Validation Snapshot:** All indoor test cases listed in `experiments/FINAL_VALIDATION_TESTS.md` have 30‑minute logs with matching ANALYSIS notes. Only the physical multi-hop spacing test (≤8 dBm) remains before declaring complete validation.

## 5. Key Innovations

### 5.1 Research Contributions

1. **First Trickle Integration with LoRaMesher**
   - Novel application of RFC 6206 to LoRa mesh
   - 31% control overhead reduction validated (44-58% achievable, 80-97% theoretical)
   - Maintains route quality with adaptive scheduling

2. **Sequence-Aware ETX Without ACK Packets**
   - Infers packet loss from sequence number gaps
   - Zero additional protocol overhead
   - EWMA smoothing for stability

3. **Multi-Factor Cost Metric**
   - Combines hop count, RSSI, SNR, ETX, gateway-bias
   - Tunable weights for deployment scenarios
   - Hysteresis prevents route flapping

4. **Hardware-Validated Research**
   - Real ESP32-S3 + SX1262 testbed
   - AS923 duty-cycle compliant
   - Reproducible experiments

### 5.2 Open-Source Contributions

**Planned Upstream Contributions:**
- Trickle scheduler implementation
- ETX tracking module
- Transmit success/failure callbacks
- Heltec V3 example code

**Repository Deliverables:**
- Complete firmware for 3 protocols
- Data collection/analysis tools
- Comprehensive documentation
- Experimental datasets

## 6. Development Workflow

### 6.1 Firmware Development

```bash
# Build specific protocol
cd firmware/1_flooding
pio run -D NODE_ID=1

# Flash to board
pio run --target upload --upload-port /dev/ttyUSB0

# Monitor output
pio device monitor --baud 115200
```

### 6.2 Data Collection

```bash
# Collect experiment data
python raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB0 \
  --output results/flooding_linear_rep1.csv \
  --protocol flooding \
  --topology linear

# Analyze results
python raspberry_pi/data_analyzer.py \
  results/flooding_linear_rep1.csv \
  --plot --report analysis/flooding_report.json
```

### 6.3 Upstream Synchronization

```bash
# Weekly sync with LoRaMesher
git fetch upstream
git checkout -b sync-upstream-$(date +%Y%m%d)
git merge upstream/main
./run_tests.sh
```

## 7. Risk Management

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Hardware failure | High | 5 boards available (redundancy) |
| Library bugs | Medium | Extensive baseline testing first |
| Time sync issues | Medium | NTP + periodic beacons |
| Duty cycle violations | High | Conservative traffic + monitoring |
| Upstream breaking changes | Low | Weekly sync + regression tests |

## 8. Timeline & Milestones

### Phase 1: Foundation (Weeks 1-2) ✅
- [x] Hardware setup and testing
- [x] Protocol 1 & 2 implementation
- [x] Basic data collection pipeline

### Phase 2: Core Development (Weeks 3-5) ✅
- [x] Trickle scheduler implementation
- [x] ETX tracking system
- [x] Cost calculation framework
- [x] Cost-based routing callback registered (needs multi-hop validation)
- [x] Hysteresis implementation (15% threshold)

### Phase 3: Testing & Analysis (Weeks 6-7) 🔄
- [x] 3-node topology experiments
- [x] 4-5 node scalability tests (max 5 nodes available)
- [x] Multi-gateway load sharing + failure cycling (Nov 13 endurance tests)
- [ ] Physical multi-hop validation (≤8 dBm hallway/parking-lot test)
- [ ] Statistical analysis (plots + comparative tables)
- [ ] Performance visualization (charts for thesis)

### Phase 4: Documentation (Week 8) 🔄
- [x] Core documentation sync (README, CLAUDE, AI instructions, Protocol 3 design notes)
- [ ] Final report compilation / thesis chapters
- [ ] Baseline ANALYSIS.md professional rewrites
- [ ] Open-source release preparation + upstream PRs

## 9. Success Criteria

### Technical Metrics
- ✅ 3 working protocol implementations (P1 flooding, P2 hop-count, P3 gateway-aware)
- ✅ 31% HELLO reduction validated (27‑31% across 3‑5 nodes, 80‑97% theoretical maximum)
- ⏳ 40% total traffic reduction vs flooding (requires multi-hop deployment to measure relay airtime)
- ✅ >95% PDR across all protocols (96.7‑100% indoors)
- ⏳ <500 ms average latency (to be characterized during hallway test)

### Deliverable Metrics
- ✅ Complete firmware codebase
- ✅ Data collection infrastructure
- ✅ Analysis tools and scripts
- ⏳ ≥60 hours of experimental data (currently 7 hours logged; remainder pending outdoor runs)
- ⏳ Statistical validation (p<0.05) for comparison plots
- 🔄 Comprehensive documentation (core docs synced; ANALYSIS.md rewrites + thesis draft pending)

### Academic Metrics
- 🔄 Internship report (in progress)
- 🔄 Presentation slides
- ✅ Reproducible experiments (14 indoor logs with tutorials)
- 🔄 Open-source release (planned upon thesis submission)
- 🔄 Community contributions (LoRaMesher PR after release)

## 10. Next Steps

### Immediate (This Week)
1. Run hallway/parking-lot multi-hop test (≤8 dBm) and produce ANALYSIS.md + plots.
2. Update README/PRD/CLAUDE/experiments docs with multi-hop evidence.
3. Continue ANALYSIS.md professional rewrites (Protocols 1/2 + Protocol 3 retests).
4. Maintain session logs + project status after each change.

### Short-term (Next 2 Weeks)
5. Generate comparative plots/tables (PDR, HELLO counts, duty cycle).
6. Draft thesis chapters (methodology, results, discussion).
7. Prepare presentation slides and defense narrative.
8. Optional: run ≥60 min soak or outdoor dual-gateway repeat for thesis appendix.

### Medium-term (Weeks 6-8)
9. Finalize documentation + thesis submission package.
10. Tag and open-source the final firmware/doc set.
11. Submit upstream LoRaMesher PR with load-serialization changes.

## 11. Contact & Resources

**Student**: Nyein Chan Win Naing
**Email**: st123843@ait.asia
**Chairperson (Advisor)**: Prof. Attaphongse Taparugssanagorn (attaphongset@ait.ac.th)
**Co-Chairperson (Internship Advisor)**: Dr. Adisorn Lertsinsrubtavee (adisorn@ait.ac.th)
**Institution**: Asian Institute of Technology (AIT)

**Repository**: https://github.com/ncwn/xMESH
**Upstream**: https://github.com/LoRaMesher/LoRaMesher
**Library Dependency**: All protocols use forked library at `https://github.com/ncwn/xMESH.git#main` (merged to main 2025-11-10)
**Documentation**: See `/docs` folder

**Hardware**:
- [Heltec WiFi LoRa 32 V3 Docs](https://docs.heltec.org/en/node/esp32/index.html)
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262)
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/)

---

*This PRD represents a comprehensive research project combining theoretical protocol design with practical hardware implementation, emphasizing empirical validation and open-source contribution to advance the state of LoRa mesh networking.*
