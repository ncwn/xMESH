# xMESH: Gateway-Aware LoRa Mesh Network

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![LoRa](https://img.shields.io/badge/LoRa-SX1262-green.svg)](https://www.semtech.com/products/wireless-rf/lora-core/sx1262)
[![LoRaMesher](https://img.shields.io/badge/LoRaMesher-Library-green.svg)](https://github.com/LoRaMesher/LoRaMesher)

A research implementation of scalable LoRa mesh networking with gateway-aware cost routing and adaptive overhead reduction. This project extends the [LoRaMesher](https://github.com/LoRaMesher/LoRaMesher) library with novel routing optimizations for duty-cycle constrained Low-Power Wide-Area Networks (LPWAN).

**Research Author:** Nyein Chan Win Naing

**Institution:** Asian Institute of Technology (AIT)

**Degree:** Master of Science in Computer Science

**Research Period:** Sep - Dec 2025

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Research Contributions](#research-contributions)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Project Structure](#project-structure)
- [Installation](#installation)
- [Usage](#usage)
  - [Protocol 1: Flooding](#protocol-1-flooding)
  - [Protocol 2: Hop-Count Routing](#protocol-2-hop-count-routing)
  - [Protocol 3: Gateway-Aware Cost Routing](#protocol-3-gateway-aware-cost-routing)
  - [Raspberry Pi Data Collection](#raspberry-pi-data-collection)
- [Configuration](#configuration)
- [Research Results](#research-results)
- [Documentation](#documentation)
- [Citation](#citation)
- [License](#license)
- [Acknowledgments](#acknowledgments)

---

## Overview

Standard LoRaWAN deployments struggle with coverage gaps due to single-hop topology limitations. While mesh networking addresses this through multi-hop relay, existing LoRa mesh protocols face critical scalability barriers from broadcast-based routing and fixed-interval control overhead.

**xMESH** implements a gateway-aware cost routing protocol that combines:
1. **Trickle Adaptive Scheduling** - Reduces HELLO overhead through exponential backoff and redundancy suppression
2. **Multi-Metric Cost Function** - Integrates signal quality (RSSI, SNR) and gateway load for intelligent path selection
3. **Proactive Fault Detection** - Safety mechanisms preventing over-suppression while enabling rapid convergence

This research demonstrates that intelligent routing can achieve **30% HELLO overhead reduction**, **96-100% packet delivery ratio** in indoor scenarios, and successful dual-gateway load distribution while maintaining LoRa duty-cycle compliance (1% for AS923 Thailand).

---

## Key Features

### Core Capabilities
- **Multi-Hop Routing**: Dynamic mesh topology with automatic route discovery
- **Three Protocol Implementations**: Comparative baseline for flooding, hop-count, and optimized gateway-aware routing
- **Adaptive Control Overhead**: Trickle-based scheduler reducing unnecessary HELLO broadcasts by 85-90%
- **Zero-Overhead ETX Tracking**: Sequence-gap detection eliminates ACK overhead for link quality assessment
- **Active Gateway Load Sharing**: Real-time load encoding enables dynamic traffic distribution across multiple gateways
- **Duty Cycle Compliance**: AS923 Thailand regulatory compliance (923.0-923.4 MHz, 1% duty cycle)

### Hardware Integration
- **ESP32-S3 Platform**: Heltec WiFi LoRa 32 V3 boards with SX1262 transceivers
- **OLED Display Support**: Real-time network statistics visualization
- **Raspberry Pi Gateway**: MQTT-based data collection and network monitoring
- **Multi-Node Testbed**: Support for 3-7 node deployments

### Monitoring & Analysis
- **Comprehensive Logging**: RSSI, SNR, hop count, routing table changes, packet flows
- **MQTT Integration**: Real-time data publishing to cloud/local brokers
- **Python Analysis Suite**: Data visualization, performance metrics, scalability analysis
- **Experimental Validation**: Indoor/outdoor test scenarios with documented results

---

## Research Contributions

This research makes six novel contributions to LPWAN mesh networking:

1. **First Complete Trickle Integration with LoRaMesher** - Adaptive scheduler achieving 85-90% suppression efficiency
2. **Local Fault Isolation Discovery** - Trickle operates as local per-node decisions rather than network-wide cascades, limiting fault impact regionally
3. **Zero-Overhead ETX Tracking** - Sequence-gap detection eliminates traditional ACK overhead
4. **Active Gateway Load Sharing** - Real-time load encoding enabling dynamic traffic distribution
5. **Safety HELLO Mechanism** - Prevents over-suppression while enabling rapid fault detection (<60s)
6. **Proactive Health Monitoring** - Reduced fault detection time versus library baseline

**Key Results:**
- ~30% HELLO overhead reduction compared to fixed-interval baseline
- 96-100% PDR in indoor multi-hop scenarios
- Successful dual-gateway load distribution with real-time cost updates
- Multi-hop validation: relay nodes forwarding traffic with cost-based path selection

---

## Hardware Requirements

| Component | Specification | Quantity |
|-----------|---------------|----------|
| **Microcontroller** | Heltec WiFi LoRa 32 V3 (ESP32-S3) | 3-7 nodes |
| **LoRa Transceiver** | Semtech SX1262 (onboard) | - |
| **Display** | 0.96" OLED (onboard) | - |
| **Gateway Host** | Raspberry Pi 3B+ or newer | 1-2 units |
| **USB Cables** | USB-C for Heltec boards | 3-7 cables |
| **Power Supply** | 5V USB power or battery | As needed |

**Optional:**
- GPS module (TinyGPS++) for location tracking
- External antennas for extended range testing

---

## Software Requirements

### Firmware Development
- **PlatformIO** (latest) - Cross-platform IDE
- **Arduino Framework** for ESP32
- **Visual Studio Code** (recommended)

### Key Libraries
- **LoRaMesher** (modified fork - this repository)
- **RadioLib** (^7.1.0) - LoRa radio driver
- **Adafruit SSD1306** (^2.5.7) - OLED display
- **Adafruit GFX Library** (^1.11.5)
- **TinyGPSPlus** (^1.0.3) - GPS support

### Data Collection (Raspberry Pi)
- **Python 3.7+**
- **pyserial** - Serial communication
- **paho-mqtt** - MQTT client
- **pandas, matplotlib** - Data analysis
- **numpy, scipy** - Statistical analysis

---

## Project Structure

```
xMESH/
├── firmware/
│   ├── 1_flooding/          # Protocol 1: Simple flooding baseline
│   │   ├── src/
│   │   │   └── main.cpp
│   │   └── platformio.ini
│   ├── 2_hopcount/          # Protocol 2: Hop-count routing baseline
│   │   ├── src/
│   │   │   └── main.cpp
│   │   └── platformio.ini
│   ├── 3_gateway_routing/   # Protocol 3: Gateway-aware cost routing
│   │   ├── src/
│   │   │   └── main.cpp
│   │   └── platformio.ini
│   └── common/              # Shared utilities and configurations
│       ├── display_utils.h
│       ├── lora_config.h
│       └── node_config.h
├── src/                     # Modified LoRaMesher library source
│   ├── LoraMesher.cpp
│   ├── LoraMesher.h
│   ├── services/            # Routing, packet, queue services
│   ├── modules/             # Display, health check modules
│   └── entities/            # Packet, routing table entities
├── raspberry_pi/            # Data collection & analysis
│   ├── serial_collector.py # Capture serial data from nodes
│   ├── mqtt_publisher.py   # Publish to MQTT broker
│   ├── data_analyzer.py    # Performance analysis
│   ├── generate_timeseries_plots.py
│   ├── multi_node_capture.py
│   └── README.md
├── final_report/            # LaTeX research documentation
│   ├── main.tex
│   ├── main.pdf
│   ├── chapters/
│   ├── appendices/
│   └── figures/
├── examples/                # Example implementations from LoRaMesher
├── library.json             # PlatformIO library manifest
├── library.properties       # Arduino library properties
└── README.md                # This file
```

---

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/ncwn/xMESH.git
cd xMESH
```

### 2. Install PlatformIO

**Via VS Code Extension:**
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install PlatformIO IDE extension
3. Restart VS Code

**Via CLI:**
```bash
pip install platformio
```

### 3. Hardware Setup

1. **Connect Heltec LoRa 32 V3 boards** via USB-C to your computer
2. **Identify USB ports** (macOS: `/dev/cu.usbserial-*`, Linux: `/dev/ttyUSB*`, Windows: `COM*`)
3. **Configure node roles** (see [Configuration](#configuration))

### 4. Flash Firmware

Navigate to desired protocol directory and upload:

```bash
cd firmware/3_gateway_routing
pio run --target upload --upload-port /dev/cu.usbserial-0001
```

Or use PlatformIO IDE's "Upload" button.

---

## Usage

### Protocol 1: Flooding

Simple broadcast-based routing where nodes rebroadcast all received packets.

**Use Case:** Baseline for demonstrating scalability limitations of broadcast approaches.

```bash
cd firmware/1_flooding
# Edit platformio.ini to set NODE_ID
pio run --target upload
pio device monitor
```

### Protocol 2: Hop-Count Routing

Default LoRaMesher proactive distance-vector protocol using hop count as sole metric.

**Use Case:** Standard metric-based routing baseline.

```bash
cd firmware/2_hopcount
pio run --target upload
pio device monitor
```

### Protocol 3: Gateway-Aware Cost Routing

Enhanced protocol with composite cost metric (RSSI, SNR, ETX, gateway-bias) and Trickle adaptive scheduling.

**Use Case:** Optimized scalable routing for production deployments.

```bash
cd firmware/3_gateway_routing
pio run --target upload
pio device monitor
```

**Key Features:**
- Multi-metric cost function
- Trickle adaptive HELLO scheduling
- Zero-overhead ETX tracking
- Active gateway load sharing
- Safety HELLO mechanism

### Raspberry Pi Data Collection

**Setup:**
```bash
cd raspberry_pi
pip install -r requirements.txt  # Create if needed: pyserial, paho-mqtt, pandas, matplotlib
```

**Configure MQTT:**
Edit `mqtt_config.json`:
```json
{
  "broker": "localhost",
  "port": 1883,
  "topic_prefix": "loramesher",
  "username": "",
  "password": ""
}
```

**Run Data Collector:**
```bash
python serial_collector.py --port /dev/ttyUSB0 --baudrate 115200
```

**Multi-Node Capture:**
```bash
python multi_node_capture.py --ports /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2
```

**Generate Analysis Plots:**
```bash
python generate_timeseries_plots.py
python data_analyzer.py
```

---

## Configuration

### Node Configuration

Edit `firmware/common/node_config.h`:

```cpp
// Node Role Assignment
#define NODE_ROLE_SENSOR    1
#define NODE_ROLE_ROUTER    2
#define NODE_ROLE_GATEWAY   3

// AS923 Thailand LoRa Settings
#define LORA_FREQUENCY      923.2   // MHz
#define LORA_BANDWIDTH      125.0   // kHz
#define LORA_SPREADING_FACTOR 7     // SF7 (max data rate)
#define LORA_CODING_RATE    5       // 4/5
#define LORA_TX_POWER       14      // dBm (under 16 dBm EIRP limit)

// Duty Cycle Compliance
#define DUTY_CYCLE_LIMIT    1.0     // 1% for AS923
```

### Protocol-Specific Settings

**Protocol 3 (Gateway Routing):**

Edit `src/LoraMesher.h` for advanced tuning:

```cpp
// Composite Cost Function Weights
#define W_HOP_COUNT     0.3
#define W_RSSI          0.2
#define W_SNR           0.2
#define W_ETX           0.2
#define W_GATEWAY_BIAS  0.1

// Trickle Parameters
#define TRICKLE_I_MIN   8000    // ms
#define TRICKLE_I_MAX   60000   // ms
#define TRICKLE_K       3       // redundancy threshold

// Safety HELLO
#define SAFETY_HELLO_INTERVAL 60000  // ms
```

---

## Research Results

### Performance Summary (Indoor Scenarios)

| Metric | Protocol 1 (Flood) | Protocol 2 (Hop) | Protocol 3 (Gateway) |
|--------|-------------------|------------------|---------------------|
| **PDR** | ~95% | ~97% | **96-100%** |
| **HELLO Overhead** | N/A | Baseline (100%) | **~70%** (30% reduction) |
| **Multi-Hop** | Limited | Yes | **Enhanced** |
| **Load Balancing** | No | No | **Dual-gateway** |
| **ETX Tracking** | No | Manual ACK | **Zero-overhead** |
| **Fault Detection** | N/A | Timeout-based | **<60s proactive** |

### Key Findings

1. **Trickle Adaptive Scheduling:**
   - 85-90% suppression efficiency in stable networks
   - Local fault isolation prevents cascading failures
   - Rapid convergence after topology changes

2. **Multi-Metric Routing:**
   - Cost-based path selection outperforms hop-count in weak-link scenarios
   - RSSI/SNR integration improves link quality awareness
   - Gateway-bias ensures traffic flows toward border nodes

3. **Scalability:**
   - Reduced control overhead enables larger mesh deployments
   - Duty-cycle compliance maintained across 3-7 node testbeds
   - Performance degradation less severe than flooding baseline

---

## Documentation

### Research Documents
- **[Final Report](/final_report/main.pdf)** - Complete results and analysis

### Technical Documentation
- **[Raspberry Pi Guide](raspberry_pi/README.md)** - Data collection setup

### LoRaMesher Documentation
- **[Official Documentation](https://jaimi5.github.io/LoRaMesher/)**
- **[GitHub Repository](https://github.com/LoRaMesher/LoRaMesher)**

---

## Citation

If you use this research in your work, please cite:

```bibtex
@mastersinternship{ncwn/xMESH,
  author       = {Nyein Chan Win Naing},
  title        = {Design and Implementation of a LoRa Mesh Network with an Optimized Routing Protocol},
  school       = {Asian Institute of Technology},
  year         = {2025},
  month        = {December},
  address      = {Pathum Thani, Thailand},
  type         = {Master's Internship Research},
  note         = {School of Engineering and Technology}
}
```

**Research Supervisors:**
- **Chairperson:** Prof. Attaphongse Taparugssanagorn
- **Co-chairperson:** Dr. Adisorn Lertsinsrubtavee

---

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

**Original LoRaMesher Library:**
LoRaMesher Contributors
Licensed under MIT License

**xMESH Research Fork:**
Nyein Chan Win Naing
Asian Institute of Technology

---

## Acknowledgments

- **Prof. Attaphongse Taparugssanagorn** - Research guidance and supervision
- **Dr. Adisorn Lertsinsrubtavee** - Technical advice and InterLAB resources
- **Asian Institute of Technology (AIT)** - Research facilities and academic support
- **LoRaMesher Contributors** - Original library foundation ([Jaimi5](https://github.com/Jaimi5), Roger Pueyo, Sergi Miralles)
- **RadioLib** - Excellent LoRa abstraction library
- **AIT InterLAB** - Experimental testbed environment

---

## Contributing

This is a research project fork. For contributing to the base LoRaMesher library, please visit:
https://github.com/LoRaMesher/LoRaMesher

---

## Future Work

Potential extensions identified in the research:

1. **Phase 2 Integration:** Connect with AIT Hazemon sensor platform
2. **Mobility Support:** Enhanced protocol for high-mobility scenarios
3. **Power Analysis:** Detailed battery lifetime characterization
4. **Security Hardening:** Cryptographic overhead evaluation
5. **Regional Expansion:** EU868, US915 frequency plan validation
6. **Larger Scale Testing:** 10+ node deployments
7. **Environmental Diversity:** Outdoor, industrial, agricultural deployments

---

**Last Updated:** November 2025

**Version:** 0.0.11 (based on LoRaMesher v0.0.11)