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
3. **Cost + Trickle + ETX** - Enhanced protocol with:
   - 🔄 **Trickle adaptive HELLO**: 90% overhead reduction
   - 📊 **ETX sliding window**: Link quality tracking with EWMA smoothing
   - 🎯 **Multi-factor cost routing**: RSSI + ETX optimization

**Hardware:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)

---

## Key Features

### Trickle Adaptive HELLO Scheduler
- **Exponential backoff**: 60s → 120s → 240s → 480s → 600s
- **90% overhead reduction**: 3 vs 30 HELLO messages per hour
- **100% PDR maintained**: No performance degradation
- **RFC 6206 compliant**: Standard Trickle algorithm implementation

### Enhanced ETX Metric
- **Sliding window**: 10-packet circular buffer
- **EWMA smoothing**: α=0.3 for stable link quality estimation
- **Continuous updates**: Real-time tracking of link conditions
- **Improved routing**: Better path selection based on delivery probability

### Multi-Factor Cost Routing
```c
Cost = 0.7 × ETX + 0.3 × normalized_RSSI
```
- Balances link quality (ETX) and signal strength (RSSI)
- Hysteresis prevents route flapping
- Gateway-aware path optimization

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
- **Trickle HELLO scheduler**: 90% overhead reduction (RFC 6206)
- **ETX sliding window**: 10-packet circular buffer with EWMA smoothing (α=0.3)
- **Multi-factor cost**: `Cost = 0.7 × ETX + 0.3 × normalized_RSSI`
- **Route stability**: Hysteresis prevents flapping
- Low overhead: O(0.8N√N) complexity with Trickle

**Use case:** Large networks (15-30 nodes), duty-cycle constrained, scalability required

**Trickle Behavior:**
- Stable network: 60s → 120s → 240s → 480s → 600s (exponential backoff)
- Topology change: Resets to 60s for fast convergence
- Result: ~6 HELLOs/hour vs 30 HELLOs/hour baseline

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
  --protocol cost_trickle \
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
├── experiments/                   # Testing and results
│   ├── results/                  # CSV data files + figures/
│   ├── CURRENT_TESTING_PLAN.md   # 🔗 Step-by-step testing guide
│   └── OCT_27_PROGRESS_SUMMARY.md # Phase 4 completion report
│
├── src/                           # LoRaMesher library source
├── docs/diagrams/                 # Architecture diagrams
│
├── README.md                      # 🌟 This file - main documentation
├── AI_HANDOFF.md                  # Context for AI assistants
├── CHANGELOG.md                   # Version history
└── IMPLEMENTATION_LOG.md          # Technical change log
└── diagrams/                 # System architecture
```

**Folder Purposes:**
- **`utilities/`**: Real-time data collection during tests (captures serial port → CSV)
- **`analysis/`**: Post-test data processing (CSV → figures, statistics, modeling)
---

**📖 Key Documentation:**
- [**Testing Procedures**](experiments/CURRENT_TESTING_PLAN.md) - Step-by-step testing guide
- [**Progress Report**](experiments/OCT_27_PROGRESS_SUMMARY.md) - Latest development status
- [**Implementation Log**](IMPLEMENTATION_LOG.md) - Technical change history
- [**AI Handoff**](AI_HANDOFF.md) - Project context for AI assistants

---

## Results

### Trickle HELLO Overhead Reduction
| Metric | Baseline | With Trickle | Improvement |
|--------|----------|--------------|-------------|
| HELLO messages/hour | 60 | 6 | **90% reduction** |
| PDR | 100% | 100% | No degradation |
| Trickle interval | - | Reaches 600s max | ✓ Converged |
| Memory usage | 324 KB | 323 KB | Stable |

### ETX Link Quality Tracking
- **Window size**: 10 packets (circular buffer)
- **EWMA smoothing**: α=0.3 (30% new, 70% history)
- **Update frequency**: Every packet (continuous)
- **Range**: 1.0 (perfect) to 10.0 (poor)

**Validation**: Window fills to 10/10, EWMA smoothing active after 3+ samples, cost function integrates ETX values correctly.

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
  --protocol cost_trickle --run-number 1 --duration 60 --output-dir ../experiments/results/
```

---

## Data Collection

The `gateway_data_collection.py` script automates data collection from the gateway node:

**Features:**
- Real-time packet reception monitoring
- Duty-cycle tracking (1% ETSI compliance)
- Memory and queue statistics
- Trickle metrics (TX count, suppression, interval)
- CSV output for analysis

**Usage:**
```bash
cd utilities
source ../venv/bin/activate
python gateway_data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --protocol [flooding|hopcount|cost_trickle] \
  --run-number 1 \
  --duration 60 \
  --output-dir ../experiments/results/
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
