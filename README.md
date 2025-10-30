# Design and Implementation of a LoRa Mesh Network with an Optimized Routing Protocol

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![LoRa: SX1262](https://img.shields.io/badge/LoRa-SX1262-green.svg)](https://www.semtech.com/products/wireless-rf/lora-core/sx1262)
[![Based on: LoRaMesher](https://img.shields.io/badge/Based%20on-LoRaMesher-orange.svg)](https://github.com/LoRaMesher/LoRaMesher)

Advanced LoRa mesh networking protocols with adaptive overhead reduction and intelligent routing for ESP32-based systems.

> **Master's Thesis Research Project** by Nyein Chan Win Naing  
> Asian Institute of Technology (AIT), School of Engineering and Technology  
> December 2025

> **Note:** This project is built on top of the [LoRaMesher](https://github.com/LoRaMesher/LoRaMesher) library and extends it with adaptive algorithms (Trickle scheduler) and enhanced link metrics (ETX sliding window with EWMA smoothing).

---

## Overview

This project implements three LoRa mesh networking protocols with progressive enhancements:

1. **Flooding** - Baseline broadcast protocol
2. **Hopcount** - Shortest path routing
3. **Cost + Trickle + ETX** *(in development)* - Enhanced protocol roadmap adds:
   - 🔄 **Trickle adaptive HELLO**: LoRaMesher HELLO cadence now driven by Trickle callbacks; hardware validation pending.
   - 📊 **ETX sliding window**: Sliding window + EWMA logic in place; needs failure reporting and routing feedback.
   - 🎯 **Multi-factor cost routing**: Cost calculation scaffolding ready; re-routing logic still TODO.

**Hardware:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)

---

## Key Features

### Trickle Adaptive HELLO Scheduler *(bench validation pending)*
- **LoRaMesher integration**: HELLO task now consults Trickle callbacks for transmission permission and per-interval delays.
- **Scheduling logic**: RFC 6206-style backoff (60s → 600s) with suppression counters and k=1 redundancy.
- **Fairness safeguards**: First HELLO is always transmitted, suppression streaks are capped, and a 5-minute safety timer guarantees a HELLO even if consistent neighbours keep the timer suppressed.
- **Next steps**: Run bench test to capture HELLO cadence vs baseline and document suppression/interval metrics.

### Enhanced ETX Metric *(routing feedback TODO)*
- **Sliding window**: 10-packet circular buffer with EWMA smoothing (α = 0.3).
- **Current status**: Success path updates implemented; failure reporting + cost feedback still needed.
- **Goal**: Use ETX to bias routing choices once full feedback loop is in place.

### Multi-Factor Cost Routing *(re-route logic pending)*
```c
Cost = W1*hops
     + W2*(1 - normalized_RSSI)
     + W3*(1 - normalized_SNR)
     + W4*(etx - 1)
     + W5*gateway_bias;
```
- Default weights: W1=1.0, W2=0.3, W3=0.2, W4=0.4, W5=1.0 (tunable).
- Helper functions normalise RSSI/SNR and maintain ETX; results are logged to serial/OLED for inspection.
- Hysteresis, gateway load balancing updates, and actual route adjustments remain TODO items.

---

## Protocol Details

### 1. Flooding Protocol (`firmware/1_flooding/`)
**Baseline broadcast protocol** where every node rebroadcasts all received packets.

**Features:**
- Maximum packet delivery (100% PDR in 3-node tests)
- Simple implementation, no routing tables
- Duplicate detection (5-entry circular cache)
- High network overhead: O(N²) complexity

**Use case:** Small networks (≤5 nodes), simple deployment, reliability critical

### 2. Hopcount Protocol (`firmware/2_hopcount/`)
**Shortest path routing** using LoRaMesher's built-in routing protocol.

**Features:**
- Automatic route discovery via HELLO packets
- Routing table maintenance
- Shortest path selection by hop count
- Medium overhead: O(N√N) complexity

**Use case:** Medium networks (≤15 nodes), balanced approach

### 3. Cost + Trickle + ETX Protocol (`firmware/3_gateway_routing/`)
**Enhanced adaptive protocol** with overhead reduction and intelligent routing.

**Features:**
- **Trickle HELLO scheduler**: RFC 6206-style timer implemented; HELLO hook + validation pending.
- **ETX sliding window**: 10-packet buffer with EWMA (α=0.3) logging link quality; routing feedback TODO.
- **Multi-factor cost**: Uses weights (W1-5) to combine hops, RSSI, SNR, ETX, gateway bias; re-route logic still under construction.
- **Route stability**: Hysteresis constants defined; activation tied to cost-based routing work.
- Goal: Achieve sub-O(N) overhead once Trickle + cost logic are fully integrated.

**Use case:** Large networks (15-30 nodes), duty-cycle constrained, scalability required

**Trickle Behavior (planned):**
- Stable network target: 60s → 120s → 240s → 480s → 600s exponential backoff.
- Topology change should reset to 60s for fast convergence once HELLO hook is in place.
- Post-integration goal: ~6 HELLOs/hour vs 30 HELLOs/hour baseline (to be validated).

---

## Quick Start

### Hardware Requirements
- 3+ Heltec WiFi LoRa 32 V3 boards
- USB-C cables for programming
- Battery power banks (optional, for untethered operation)

### Software Setup

1. **Clone repository**
```bash
```bash
git clone https://github.com/ncwn/xMESH.git
cd xMESH
```

2. **Install PlatformIO**
```bash
pip install platformio
```

3. **Build firmware**
```bash
cd firmware/3_gateway_routing
platformio run -e gateway -e router -e sensor
```

4. **Flash nodes** (one at a time, swap USB connections)
```bash
# Gateway node
platformio run -e gateway -t upload

# Router node
platformio run -e router -t upload

# Sensor node
platformio run -e sensor -t upload
```

5. **Collect data** (60-minute test run)
```bash
cd ../../utilities
source ../venv/bin/activate
python gateway_data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --protocol cost_routing \
  --run-number 1 \
  --duration 60 \
  --output-dir ../experiments/results/
```

---

## Repository Structure

```
LoRa-Mesh-Network/
├── firmware/                      # Protocol implementations (3 variants)
│   ├── 1_flooding/               # Baseline: Broadcast flooding (O(N²))
│   ├── 2_hopcount/               # Baseline: Hopcount routing (O(N√N))
│   ├── 3_gateway_routing/        # Enhanced: Cost+Trickle+ETX (O(0.8N√N))
│   └── common/                   # Shared Heltec V3 hardware config
│
├── utilities/                     # 🔧 Data collection (real-time monitoring)
│   └── gateway_data_collection.py  # Captures serial output → CSV
│
├── analysis/                      # 📊 Data analysis (post-processing)
│   ├── scalability_model.py      # Extrapolate 3-node → 10-100 nodes
│   ├── gateway_performance_analysis.py  # Week 1 comparison
│   └── performance_analysis.py   # Week 6-7 comparison
│
├── experiments/                   # Testing configs + captured results
│   ├── configs/                  # Experiment parameter templates
│   └── results/                  # CSV datasets + generated figures
│
├── src/                           # LoRaMesher library source
├── docs/                          # Additional documentation + diagrams
├── proposal_docs/                 # Thesis proposal, notes, and diagrams (local only, gitignored)
├── README.md                      # 🌟 This file - main documentation
├── AI_HANDOFF.md                  # Local AI handoff notes (gitignored)
└── CHANGELOG.md                   # Version history
```

**Folder Purposes:**
- **`utilities/`** – Real-time data collection during tests (captures serial port → CSV).
- **`analysis/`** – Post-test data processing (CSV → figures, statistics, modeling).
- **`experiments/`** – Parameter configs plus archived CSV datasets/figures from runs.
- **`proposal_docs/`** – Thesis proposal draft, presentation notes, Mermaid diagrams (local only, not committed).
---

**📖 Key Documentation:**
- [**CHANGELOG.md**](CHANGELOG.md) – Version history & milestone notes.
- **AI_HANDOFF.md** – Local handoff context for AI collaborators (directory is gitignored).
- [**proposal_docs/**](proposal_docs/) – Thesis proposal, presentation notes, and diagram sources (kept local; repo ignores this directory).

---

## Validation Plan

### Trickle HELLO Overhead Reduction *(to measure)*
- Baseline reference: LoRaMesher HELLO interval fixed at 120s (~60 packets/hour).
- Target: Demonstrate reduced HELLO transmissions once Trickle timer controls LoRaMesher HELLO scheduling.
- Actions: ✅ Timer integrated with LoRaMesher → collect before/after HELLO metrics → update documentation with measured reduction.

### ETX Link Quality Tracking *(current status)*
- Implementation: 10-packet circular buffer with α=0.3 EWMA smoothing for link ETX.
- Outstanding work: Add loss/failure reporting and drive routing decisions with updated ETX values.
- Post-task: Validate ETX responsiveness under induced packet loss and document results.

**Status note**: Success-only runs populate the window; repeat validation after adding failure handling and routing feedback.

**Bench evidence:** Trickle run (`run2_gateway.csv`) – 1 HELLO TX, 0 forced suppressions, interval → 600 s, 28 packets received. Baseline run (`run3_gateway.csv`, Trickle disabled) – 30 packets, fixed 120 s cadence. Safety timer + ETX loss logging validated in `seq-aware-etx/run1_gateway.csv` (forced loss window → ETX 1.15, automatic HELLO refresh every ≤5 min).

---

## Phase 4 Integration Roadmap

Follow these phases in order; lock in the implementation and run spot tests before moving on.

1. **Trickle → HELLO Integration**
   - Implementation: Connect the Trickle timer callbacks to LoRaMesher’s HELLO emission so adaptive intervals actually gate broadcasts.
   - Test: 15 min cold-start bench (`experiments/results/new_bench/cost_trickle/run2_gateway.csv`) produced 1 HELLO TX, 0 forced suppressions, final interval 600 s while still receiving 28 sensor packets; baseline fixed-interval run (`run3_gateway.csv`, `TRICKLE_ENABLED false`) delivered 30 packets.
   - Status: ✅ Implementation + bench validation complete.

2. **ETX Failure Reporting Loop**
   - Implementation: Capture ACK/timeout results (or equivalent) for each transmission, feed failures into the sliding window, and log ETX changes.
   - Test: Inject controlled packet loss (e.g., reduced TX power or deliberate interference) and verify ETX rises while recovery follows when link stabilises.

3. **Cost-Based Re-routing & Gateway Load**
   - Implementation: Increment gateway load counters, apply hysteresis thresholds, and issue route updates when the composite cost beats the incumbent path.
   - Test: Construct a two-path scenario (e.g., router near/far) to ensure routes switch only when the superior cost persists and chatter is avoided.

4. **Full Hardware Regression**
   - Implementation: Reflash all three protocols, then run 60-minute data-collection sessions (`gateway_data_collection.py`) for flooding, hopcount, and cost_routing.
   - Test: Confirm CSV logs include Trickle stats, queue/memory metrics, and updated ETX values; archive results under `experiments/results/`.

5. **Analysis & Documentation Refresh**
   - Implementation: Regenerate figures with the refreshed datasets, update README/CHANGELOG with measured improvements, and fold findings into thesis drafts.
   - Test: Sanity-check plots/tables against the new CSVs and capture any regressions before declaring Phase 4 complete.

---

## Hardware Configuration

### Heltec WiFi LoRa 32 V3
- **MCU**: ESP32-S3 (Dual-Core Xtensa LX7, 240 MHz)
- **LoRa**: Semtech SX1262 transceiver
- **Display**: 0.96" OLED (128×64, SSD1306)
- **Memory**: 8 MB PSRAM, 8 MB Flash

### LoRa Settings (AS923)
```cpp
Frequency:        923.2 MHz
Spreading Factor: SF7
Bandwidth:        125 kHz
Coding Rate:      4/5
TX Power:         10 dBm
Time-on-Air:      ~56ms (50-byte packet)
```

---

## Protocol Comparison

Test all three protocols to compare performance:

```bash
# Test 1: Flooding (60 min)
cd firmware/1_flooding
platformio run -e gateway -t upload  # Flash each node
platformio run -e router -t upload
platformio run -e sensor -t upload
cd ../../utilities
source ../venv/bin/activate
python gateway_data_collection.py --port /dev/cu.usbserial-0001 \
  --protocol flooding --run-number 1 --duration 60 --output-dir ../experiments/results/

# Test 2: Hopcount (60 min)
cd ../firmware/2_hopcount
platformio run -e gateway -e router -e sensor  # Build first
platformio run -e gateway -t upload  # Flash each node
platformio run -e router -t upload
platformio run -e sensor -t upload
cd ../../utilities
python gateway_data_collection.py --port /dev/cu.usbserial-0001 \
  --protocol hopcount --run-number 1 --duration 60 --output-dir ../experiments/results/

# Test 3: Cost+Trickle+ETX (60 min)
cd ../firmware/3_gateway_routing
platformio run -e gateway -t upload  # Flash each node
platformio run -e router -t upload
platformio run -e sensor -t upload
cd ../../utilities
python gateway_data_collection.py --port /dev/cu.usbserial-0001 \
  --protocol cost_routing --run-number 1 --duration 60 --output-dir ../experiments/results/
```

---

## Data Collection

The `gateway_data_collection.py` script automates data collection from the gateway node:

**Features:**
- Real-time packet reception monitoring
- Duty-cycle tracking (1% ETSI compliance)
- Memory and queue statistics
- Trickle metrics (TX count, suppression, interval)
- ETX updates + loss notices (address, window fill, instantaneous ETX) for seq-aware testing
- CSV output for analysis

**Usage:**
```bash
cd utilities
source ../venv/bin/activate
python gateway_data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --protocol [flooding|hopcount|cost_routing|cost_trickle|seq-aware-etx] \
  --run-number 1 \
  --duration 60 \
  --output-dir ../experiments/results/

# Example (seq-aware ETX bench)
python gateway_data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --protocol seq-aware-etx \
  --run-number 1 \
  --duration 15 \
  --output-dir ../experiments/results/new_bench
```

**Output:** CSV file with columns for timestamp, event type, duty-cycle, memory, packet data, routing table size, and Trickle statistics.

**Dependencies:**
```bash
pip install pyserial pandas
```

---

## Analysis Scripts

Python scripts for post-test data analysis (`analysis/` directory):

### 1. Scalability Model (`scalability_model.py`)
**Purpose:** Extrapolate 3-node hardware results to predict 10-100 node behavior.

**Usage:**
```bash
python3 analysis/scalability_model.py \
  --data-dir experiments/results \
  --output-dir experiments/results/figures
```

**Generates:**
- `scalability_duty_cycle.png` - Duty-cycle vs node count (1% ETSI limit)
- `scalability_overhead.png` - Protocol complexity (O(N²) vs O(N√N))
- `scalability_memory.png` - Memory usage projection
- `breakpoint_analysis.csv` - Maximum network sizes per protocol

### 2. Gateway Performance Analysis (`gateway_performance_analysis.py`)
**Purpose:** Compare hardware test results (Week 1: Oct 21-22 testing).

**Usage:**
```bash
python3 analysis/gateway_performance_analysis.py \
  --data-dir experiments/results \
  --output-dir experiments/results/figures
```

**Generates:** PDR, latency, and overhead comparison charts.

### 3. Performance Analysis (`performance_analysis.py`)
**Purpose:** Statistical comparison with significance testing (Week 6-7 analysis).

**Usage:**
```bash
python3 analysis/performance_analysis.py \
  --data-dir experiments/results \
  --output-dir experiments/results/figures
```

**Generates:** Statistical tests (ANOVA, t-tests), correlation analysis.

**Dependencies:**
```bash
pip install pandas numpy matplotlib seaborn scipy
# Or use requirements file:
pip install -r analysis/requirements.txt
```

---

## Development

### Building Specific Protocol
```bash
cd firmware/[protocol_directory]
platformio run -e [sensor|router|gateway]
```

*Notes:*
- `firmware/3_gateway_routing` depends on the vendored LoRaMesher copy in the repo root (`../../`) to access the HELLO scheduling callbacks.
- `firmware/1_flooding` and `firmware/2_hopcount` continue to consume the upstream LoRaMesher repository so baseline behaviour matches the original library.

### Monitoring Serial Output
```bash
platformio device monitor -b 115200
```

### Flashing Firmware
```bash
platformio run -e gateway -t upload --upload-port /dev/cu.usbserial-0001
```

---

## Acknowledgments

This project is built on top of the [LoRaMesher](https://github.com/LoRaMesher/LoRaMesher) library. Special thanks to the LoRaMesher team for providing the foundational mesh networking implementation.

**LoRaMesher Library:**
- GitHub: https://github.com/LoRaMesher/LoRaMesher
- Used for: Core routing functionality, packet handling, and LoRa radio management

## Acknowledgments

**Based on LoRaMesher Library:**
- Trickle adaptive HELLO scheduler (RFC 6206)
- ETX sliding window with EWMA smoothing
- Multi-factor cost routing optimization

---

## License

MIT License - see [LICENSE](LICENSE) file for details.

This project uses the LoRaMesher library, which is also released under MIT License.

---

## Citation

If you use this work in your research, please cite:

```bibtex
@mastersthesis{naing2025lora,
  author = {Nyein Chan Win Naing},
  title = {Design and Implementation of a LoRa Mesh Network with an Optimized Routing Protocol},
  school = {Asian Institute of Technology},
  year = {2025},
  type = {Master's Thesis},
  address = {Thailand}
}
  month = {December}
}
```

---

## Contact

**Author**: Nyein Chan Win Naing
**Email**: st123843@ait.asia
**Institution**: Asian Institute of Technology, Thailand
