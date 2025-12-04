# xMESH Raspberry Pi Data Collection Suite

Professional-grade data collection, logging, and analysis tools for the xMESH LoRa mesh networking platform. This suite enables real-time monitoring, multi-node data capture, MQTT integration, and comprehensive post-experiment analysis.

## Overview

The xMESH Raspberry Pi suite provides a complete data pipeline for LoRa mesh experiments:

- **Gateway Host**: Raspberry Pi serves as USB serial bridge to ESP32-LoRa gateway nodes
- **Data Collection**: Simultaneous capture from multiple nodes with synchronized timestamps
- **MQTT Bridge**: Real-time streaming to MQTT brokers for cloud monitoring (AIT Hazemon integration)
- **Data Logging**: CSV, JSON, and SQLite database storage with structured schemas
- **Analysis Tools**: Statistical analysis, PDR calculation, protocol comparison, visualization
- **Time-Series Plotting**: Publication-quality plots for academic papers

## System Architecture

```
┌───────────────────────────────────────────────────────────┐
│                     Raspberry Pi Host                     │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ ESP32 Node 1 │  │ ESP32 Node 2 │  │ ESP32 Node N │     │
│  │  (USB Serial)│  │  (USB Serial)│  │  (USB Serial)│     │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │
│         │                 │                 │             │
│         └─────────────────┴─────────────────┘             │
│                           │                               │
│            ┌──────────────▼──────────────┐                │
│            │  multi_node_capture.py      │                │
│            │  (Parallel Serial Capture)  │                │
│            └──────────────┬──────────────┘                │
│                           │                               │
│         ┌─────────────────┼─────────────────┐             │
│         │                 │                 │             │
│         ▼                 ▼                 ▼             │
│   ┌─────────┐      ┌──────────┐       ┌─────────┐         │
│   │ .log    │      │  .csv    │       │ .json   │         │
│   │ Files   │      │  Files   │       │ Files   │         │
│   └────┬────┘      └─────┬────┘       └────┬────┘         │
│        │                 │                 │              │
│        └─────────────────┴─────────────────┘              │
│                          │                                │
│         ┌────────────────┴────────────────┐               │
│         │                                 │               │
│         ▼                                 ▼               │
│  ┌──────────────┐               ┌──────────────┐          │
│  │ MQTT Publish │               │ Data Analyze │          │
│  │ (Real-time)  │               │ (Offline)    │          │
│  └──────┬───────┘               └──────┬───────┘          │
│         │                              │                  │
└─────────┼──────────────────────────────┼──────────────────┘
          │                              │
          ▼                              ▼
    ┌──────────┐                  ┌──────────┐
    │  MQTT    │                  │  Plots   │
    │  Broker  │                  │  Metrics │
    │ (Cloud)  │                  │  Reports │
    └──────────┘                  └──────────┘
```

## Tools Overview

| Script | Purpose | Primary Use Case |
|--------|---------|------------------|
| `multi_node_capture.py` | Parallel capture from multiple nodes | Experiment data collection |
| `serial_collector.py` | Single-node serial capture with DB/MQTT | Individual node debugging, production deployment |
| `mqtt_publisher.py` | LoRa-to-MQTT bridge for sensor data | Real-time monitoring, cloud integration |
| `data_analyzer.py` | Statistical analysis of test results | Post-experiment analysis, protocol comparison |
| `generate_timeseries_plots.py` | Publication-quality time-series plots | Academic papers, research figures |
| `test_health_check.py` | Validate test data completeness | Quality assurance |

---

## Hardware Requirements

### Recommended Setup

- **Raspberry Pi**: Model 3B+ or newer (4GB RAM recommended for long-duration tests)
- **Operating System**: Raspberry Pi OS (64-bit) or Ubuntu 20.04+
- **USB Hub**: Powered USB 3.0 hub for connecting multiple ESP32 nodes
- **USB Cables**: High-quality USB-C cables (avoid cheap cables that cause connection drops)
- **Power Supply**: 5V/3A minimum (5V/5A recommended for Pi + multiple nodes)
- **Storage**: 32GB+ microSD card (Class 10 or better) or USB SSD for intensive logging

### Tested Configurations

| Configuration | Nodes | Duration | Storage Required |
|---------------|-------|----------|------------------|
| Basic Testing | 3 nodes | 30 min | ~50 MB |
| Extended Validation | 5 nodes | 3 hours | ~500 MB |
| Long-Term Deployment | 2 nodes (gateways) | 24 hours | ~2 GB |

### Serial Port Identification

On Raspberry Pi, USB devices appear as `/dev/ttyUSB0`, `/dev/ttyUSB1`, etc.
On macOS (for development), they appear as `/dev/cu.usbserial-XXXX`.

```bash
# List all connected USB serial devices
ls /dev/ttyUSB*   # Linux/Raspberry Pi
ls /dev/cu.*      # macOS

# Identify which port corresponds to which node
# (check ESP32 serial output or use labels)
python3 -m serial.tools.list_ports
```

---

## Software Dependencies

### Required Python Packages

```bash
# Core dependencies
pip3 install pyserial>=3.5
pip3 install pandas>=1.3.0
pip3 install numpy>=1.21.0

# For MQTT integration
pip3 install paho-mqtt>=1.6.0

# For plotting and analysis
pip3 install matplotlib>=3.4.0
pip3 install seaborn>=0.11.0
pip3 install scipy>=1.7.0

# For SSL/TLS MQTT connections
pip3 install certifi
```

### Installation via requirements.txt

Create a `requirements.txt` file with:

```txt
pyserial>=3.5
pandas>=1.3.0
numpy>=1.21.0
paho-mqtt>=1.6.0
matplotlib>=3.4.0
seaborn>=0.11.0
scipy>=1.7.0
certifi>=2021.10.8
```

Then install:

```bash
pip3 install -r raspberry_pi/requirements.txt
```

### System Permissions

Grant serial port access (required on Linux/Raspberry Pi):

```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Logout and login for changes to take effect
# Verify group membership
groups
```

---

## Quick Start

### 1. Clone and Setup

```bash
# Clone repository
git clone https://github.com/yourusername/xMESH-1.git
cd xMESH-1

# Install dependencies
pip3 install -r raspberry_pi/requirements.txt

# Verify serial ports
ls /dev/ttyUSB*
```

### 2. Run a Simple Test

```bash
# Capture from 3 nodes for 5 minutes
python3 raspberry_pi/multi_node_capture.py \
  --nodes 1,3,5 \
  --ports /dev/ttyUSB0,/dev/ttyUSB1,/dev/ttyUSB2 \
  --duration 300 \
  --output experiments/results/quicktest_$(date +%Y%m%d_%H%M%S)

# Analyze results
python3 raspberry_pi/data_analyzer.py \
  experiments/results/quicktest_*/node*.log \
  --plot --report
```

### 3. Enable MQTT Monitoring (Optional)

```bash
# Edit MQTT configuration
nano raspberry_pi/mqtt_config.json

# Run MQTT publisher
python3 raspberry_pi/mqtt_publisher.py \
  --port /dev/ttyUSB0 \
  --config raspberry_pi/mqtt_config.json
```

---

## 1. Multi-Node Capture (`multi_node_capture.py`)

Captures serial output from multiple ESP32 nodes in parallel, saving each to a separate log file.

### Features
- Parallel serial capture from up to 5 nodes
- Real-time statistics (TX/RX counts, Trickle metrics)
- Automatic timestamping and folder creation
- Graceful shutdown on Ctrl+C

### Usage

```bash
# Basic 3-node capture (300 seconds)
python3 raspberry_pi/multi_node_capture.py \
  --nodes 1,3,5 \
  --ports /dev/cu.usbserial-0001,/dev/cu.usbserial-5,/dev/cu.usbserial-7 \
  --baudrate 115200 \
  --duration 300 \
  --output experiments/results/protocol3/test_$(date +%Y%m%d_%H%M%S)

# Long-duration test (2 hours)
python3 raspberry_pi/multi_node_capture.py \
  --nodes 1,2,3,4,5 \
  --ports /dev/cu.usbserial-0001,/dev/cu.usbserial-2,/dev/cu.usbserial-5,/dev/cu.usbserial-6,/dev/cu.usbserial-7 \
  --baudrate 115200 \
  --duration 7200 \
  --output experiments/results/protocol3/extended_$(date +%Y%m%d_%H%M%S)
```

### Parameters

| Parameter | Description | Example |
|-----------|-------------|---------|
| `--nodes` | Comma-separated node IDs | `1,3,5` |
| `--ports` | Serial port paths (must match node order) | `/dev/cu.usbserial-0001,/dev/cu.usbserial-5,...` |
| `--baudrate` | Serial baud rate (default: 115200) | `115200` |
| `--duration` | Test duration in seconds | `300` (5 minutes) |
| `--output` | Output folder path | `experiments/results/protocol3/test_20251110_150000` |

### Output Format

Creates folder with one log file per node:
```
experiments/results/protocol3/test_20251110_150000/
├── node1.log    # Sensor node
├── node3.log    # Relay node
└── node5.log    # Gateway node
```

### Real-Time Metrics

Displays live statistics during capture:
```
=== Multi-Node Capture Status ===
Node 1: 1247 lines, 42 TX, 38 RX
Node 3: 1532 lines, 65 TX, 58 RX
Node 5: 1891 lines, 12 TX, 119 RX (GATEWAY)

Protocol 3 Trickle Metrics:
  Trickle HELLOs: 7
  Suppressions: 41
  Doubles: 5
  Resets: 2

Elapsed: 01:23:45 / 02:00:00
```

---

## 2. Data Analyzer (`data_analyzer.py`)

Processes captured logs to calculate PDR, throughput, latency, and other metrics.

### Features
- Packet Delivery Ratio (PDR) calculation
- Throughput and latency analysis
- Trickle overhead measurement
- ETX and cost convergence tracking
- CSV and JSON output
- Optional matplotlib plots

### Usage

```bash
# Analyze single test
python3 raspberry_pi/data_analyzer.py \
  experiments/results/protocol3/test_20251110_150000/*.log \
  --plot --report

# Compare multiple tests
python3 raspberry_pi/data_analyzer.py \
  experiments/results/protocol3/test_*/node*.log \
  --compare --output comparison_results.json

# Generate only plots (no console output)
python3 raspberry_pi/data_analyzer.py \
  experiments/results/protocol3/test_20251110_150000/*.log \
  --plot --quiet
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `--plot` | Generate matplotlib plots (PNG files) |
| `--report` | Print detailed statistics to console |
| `--compare` | Compare multiple test runs |
| `--output FILE` | Save results to JSON file |
| `--quiet` | Suppress console output |

### Output Metrics

**Packet Delivery Ratio (PDR)**:
```
PDR = (Packets received at gateway) / (Packets sent by sensors) × 100%
Target: >95%
```

**Control Overhead**:
```
Overhead = Total HELLO packets transmitted / Test duration (hours)
Protocol 2 baseline: ~30 HELLOs/hour
Protocol 3 target: <12 HELLOs/hour (60% reduction)
```

**Throughput**:
```
Throughput = Total data bytes delivered / Test duration (seconds)
```

**Latency** (if timestamps available):
```
Latency = Gateway RX timestamp - Sensor TX timestamp
Statistics: min, max, mean, median, p95, p99
```

### Example Output

```
=== Test Results ===
Test: experiments/results/protocol3/test_20251110_150000

Packet Delivery Ratio: 100.0% (119/119 packets) ✅
Control Overhead: 6.25 HELLOs/hour (7 total in 2 hours)
  Reduction vs Protocol 2: 79.2% (baseline: 30 HELLOs/hour)

Throughput: 1.2 KB/hour
Average Latency: 450 ms (median: 380 ms)

Trickle Performance:
  Trickle HELLOs: 7 (9.3%)
  Safety HELLOs: 68 (90.7%)
  Suppressions: 41 (85% efficiency)
  I_max sustained: 105 minutes

Cost Convergence:
  Initial: 1.39
  Final: 1.18
  Improvement: -15.3%
```

---

## 3. Serial Collector (`serial_collector.py`)

Advanced single-node serial capture with SQLite database and MQTT integration. Ideal for production deployments and long-term monitoring.

### Features

- **Multiple Output Formats**: CSV, SQLite database, MQTT streaming
- **Real-time MQTT Publishing**: Stream data to cloud brokers for monitoring
- **Structured Database**: SQLite schema for experiments, packets, and metrics
- **CSV Data Logging**: Compatible with pandas and Excel for post-processing
- **Statistics Tracking**: Uptime, packet counts, error rates

### Basic Usage

```bash
# Simple CSV logging
python3 raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --output gateway_data.csv

# With SQLite database
python3 raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB0 \
  --output gateway_data.csv \
  --database gateway_data.db

# With MQTT publishing
python3 raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB0 \
  --output gateway_data.csv \
  --mqtt-broker mqtt.example.com \
  --mqtt-port 1883 \
  --mqtt-user myuser \
  --mqtt-pass mypass
```

### Advanced Usage

```bash
# Production deployment with all features
python3 raspberry_pi/serial_collector.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --output /var/log/xmesh/gateway_$(date +%Y%m%d).csv \
  --database /var/lib/xmesh/experiments.db \
  --mqtt-broker mqtt.example.com \
  --mqtt-port 8883 \
  --mqtt-user student \
  --mqtt-pass secretpass \
  --protocol gateway_routing \
  --topology outdoor_deployment \
  --repetition 1
```

### Output Formats

**CSV Format** (gateway_data.csv):
```csv
timestamp,node_id,event_type,src,dest,rssi,snr,etx,hop_count,packet_size,sequence,cost,next_hop,gateway
1699732421123,5,RX,1,5,-85.2,8.5,1.2,2,64,42,1.39,3,5
1699732422456,5,RX,3,5,-78.1,9.8,1.1,1,64,43,1.18,5,5
```

**SQLite Schema**:

- `experiments`: Experiment metadata (start_time, protocol, topology)
- `packets`: Per-packet data (timestamp, RSSI, SNR, ETX, hop_count)
- `metrics`: Aggregated metrics (PDR, throughput, latency)

**MQTT Topics**:

- `xmesh/node/{node_id}/packet`: Individual packet data (JSON)
- `xmesh/stats`: Collector statistics (packets received, errors, uptime)

### When to Use

- **Production Deployment**: Long-term monitoring with database storage
- **Cloud Integration**: Real-time data streaming to MQTT dashboards
- **Individual Node Debugging**: Single-node troubleshooting
- **Gateway Monitoring**: Continuous gateway operation tracking

---

## 4. MQTT Publisher (`mqtt_publisher.py`)

Dedicated LoRa-to-MQTT bridge for publishing sensor data to cloud platforms. Designed for AIT Hazemon infrastructure integration.

### Features

- **Sensor Data Parsing**: Extracts PM2.5, GPS coordinates from gateway logs
- **JSON Payload Construction**: Structured data for cloud ingestion
- **TLS/SSL Support**: Secure connections to MQTT brokers
- **QoS Control**: Configurable Quality of Service levels
- **Multi-Node Routing**: Publishes data from multiple sensor nodes
- **Real-time Statistics**: Connection status, publish counts, error tracking

### Configuration

Edit `mqtt_config.json`:

```json
{
  "broker": "mqtt.example.com",
  "port": 8883,
  "username": "your_username",
  "password": "your_password",
  "client_id": "xmesh_gateway_publisher",
  "qos": 1,
  "use_tls": true,
  "tls_insecure": false,
  "topics": {
    "sensor_data": "mesh/ingest/{node_id}"
  },
  "keepalive": 60,
  "clean_session": true
}
```

### Usage

```bash
# Basic usage (localhost MQTT broker)
python3 raspberry_pi/mqtt_publisher.py \
  --port /dev/ttyUSB0 \
  --config raspberry_pi/mqtt_config.json

# Production deployment with custom config
python3 raspberry_pi/mqtt_publisher.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --config /etc/xmesh/mqtt_config.json
```

### Data Flow

```
Gateway Serial Output:
  RX: Seq=42 From=BB94
    PM: 1.0=12 2.5=15 10=18 µg/m³ (AQI: Good)
    GPS: 13.727800°N, 100.775300°E, alt=25.5m, 7 sats

↓ Parsed and Published to MQTT ↓

Topic: mesh/ingest/BB94
Payload:
{
  "timestamp": "2025-11-23T14:32:15.123",
  "sequence": 42,
  "source": "BB94",
  "pm1_0": 12.0,
  "pm2_5": 15.0,
  "pm10": 18.0,
  "latitude": 13.7278,
  "longitude": 100.7753,
  "altitude": 25.5,
  "satellites": 7,
  "gps_valid": true
}
```

### Monitoring Output

```
╔═══════════════════════════════════════════════════════════╗
║      xMESH MQTT Publisher for AIT Hazemon                 ║
╚═══════════════════════════════════════════════════════════╝

✅ Connected to gateway: /dev/ttyUSB0 @ 115200 baud
✅ Connected to MQTT broker: mqtt.ait.ac.th
   Topics: mesh/ingest/{node_id}

📡 Listening for sensor data from gateway...

Press Ctrl+C to stop

RX: Seq=1 From=BB94
  PM: 1.0=12 2.5=15 10=18 µg/m³ (AQI: Good)
  GPS: 13.727800°N, 100.775300°E, alt=25.5m, 7 sats (Excellent)
📤 Published to mesh/ingest/BB94: Seq=1

📊 Stats: 10 RX, 10 published, 0 errors, 120s uptime
```

### Integration with Firmware

The MQTT publisher expects **Protocol 3 (Gateway Routing)** firmware output format:

```cpp
// Firmware prints to Serial (in firmware/3_gateway_routing/src/main.cpp)
Serial.printf("RX: Seq=%d From=%04X\n", packet.seq, packet.src);
Serial.printf("  PM: 1.0=%.1f 2.5=%.1f 10=%.1f µg/m³\n",
              pm1_0, pm2_5, pm10);
Serial.printf("  GPS: %.6f°N, %.6f°E, alt=%.1fm, %d sats\n",
              lat, lon, alt, sats);
```

### Running as System Service

Create `/etc/systemd/system/xmesh-mqtt.service`:

```ini
[Unit]
Description=xMESH MQTT Publisher
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/xMESH-1
ExecStart=/usr/bin/python3 /home/pi/xMESH-1/raspberry_pi/mqtt_publisher.py \
  --port /dev/ttyUSB0 \
  --config /home/pi/xMESH-1/raspberry_pi/mqtt_config.json
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable xmesh-mqtt.service
sudo systemctl start xmesh-mqtt.service
sudo systemctl status xmesh-mqtt.service
```

---

## 5. Time-Series Plot Generator (`generate_timeseries_plots.py`)

Generates publication-quality time-series plots from experimental logs for academic papers and research documentation.

### Features

- **Trickle Interval Progression**: Visualize adaptive interval growth from I_min to I_max
- **HELLO Overhead Comparison**: Protocol 2 vs Protocol 3 cumulative HELLO counts
- **ETX Evolution**: Link quality tracking over time for multiple links
- **Gateway Load Distribution**: Dual-gateway load balancing visualization
- **Publication Quality**: 300 DPI output, serif fonts, proper axis labels
- **Automated Analysis**: Direct parsing from experiment log files

### Generated Plots

1. **Trickle Interval Progression Over Time**
   - Shows interval doubling: 60s → 120s → 240s → 480s → 600s
   - Marks I_min, I_max, and Safety HELLO ceiling
   - Annotates convergence time to I_max

2. **HELLO Overhead Comparison (Protocol 2 vs 3)**
   - Cumulative HELLO count comparison
   - Calculates percentage reduction
   - Overlays theoretical and actual values

3. **ETX Link Quality Evolution**
   - Multi-link ETX tracking
   - ETX = 1.0 (perfect) and 2.0 (marginal) reference lines
   - Outdoor test data with variable link quality

4. **Gateway Load Distribution Over Time**
   - Packet arrival rate per gateway
   - Switch threshold visualization
   - Load balancing effectiveness

### Usage

```bash
# Generate all plots
python3 raspberry_pi/generate_timeseries_plots.py

# Output: proposal_docs/images/figure_timeseries_*.png
```

### Customization

Edit `generate_timeseries_plots.py` to change:

- **Input Log Files**: Update file paths to your experiment directories
- **Plot Style**: Modify `plt.rcParams` for different aesthetics
- **Time Range**: Adjust `offset` and `limit` parameters
- **Colors**: Change color schemes for different protocols/nodes

### Example Output

```
Generating time-series plots from test logs...

✅ Trickle interval progression saved: proposal_docs/images/figure_timeseries_trickle_interval.png
✅ HELLO cumulative comparison saved: proposal_docs/images/figure_timeseries_hello_cumulative.png
✅ ETX time-series saved: proposal_docs/images/figure_timeseries_etx_evolution.png
✅ Gateway load time-series saved: proposal_docs/images/figure_timeseries_gateway_load.png

✅ All time-series plots generated successfully!
Output directory: proposal_docs/images/
```

---

## 6. Test Health Check (`test_health_check.py`)

Validates test data completeness and identifies issues.

### Features
- Detects missing nodes
- Identifies gaps in data
- Validates PDR calculation inputs
- Reports warnings and errors

### Usage

```bash
# Check test validity
python3 raspberry_pi/test_health_check.py \
  experiments/results/protocol3/test_20251110_150000/

# Check multiple tests
for dir in experiments/results/protocol3/test_*/; do
  python3 raspberry_pi/test_health_check.py "$dir"
done
```

### Output

```
=== Test Health Check ===
Test: experiments/results/protocol3/test_20251110_150000

✅ All expected nodes present (1, 3, 5)
✅ No data gaps detected
✅ PDR calculation inputs valid
⚠️  Node 3 TX count low (expected relay forwarding)
✅ Test duration matches target (7200s ±2%)

Status: PASS (1 warning)
```

---

## Common Workflows

### Workflow 1: Comprehensive Test Run

```bash
# 1. Create timestamped test folder
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
TEST_DIR="experiments/results/protocol3/test_$TIMESTAMP"

# 2. Flash all nodes (from firmware directory)
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001
./flash_node.sh 3 /dev/cu.usbserial-5
./flash_node.sh 5 /dev/cu.usbserial-7
cd ../..

# 3. Capture data (5 minutes)
python3 raspberry_pi/multi_node_capture.py \
  --nodes 1,3,5 \
  --ports /dev/cu.usbserial-0001,/dev/cu.usbserial-5,/dev/cu.usbserial-7 \
  --duration 300 \
  --output "$TEST_DIR"

# 4. Validate data
python3 raspberry_pi/test_health_check.py "$TEST_DIR"

# 5. Analyze results
python3 raspberry_pi/data_analyzer.py "$TEST_DIR"/*.log --plot --report
```

### Workflow 2: Protocol Comparison

```bash
# Analyze all three protocols
python3 raspberry_pi/data_analyzer.py \
  experiments/results/protocol1/test_*/node*.log \
  experiments/results/protocol2/test_*/node*.log \
  experiments/results/protocol3/test_*/node*.log \
  --compare --output protocol_comparison.json --plot

# Generates:
# - protocol_comparison.json (statistical data)
# - pdr_comparison.png
# - overhead_comparison.png
# - latency_comparison.png
```

---

## Data Formats and Schemas

### Log File Format (Multi-Node Capture)

Each node log contains timestamped serial output with parsed events:

```
=== Node 1 Capture Log ===
Port: /dev/ttyUSB0
Started: 2025-11-23T14:30:00.000
==================================================

[14:30:01.123] xMESH Node 1 (Sensor) - Protocol 3
[14:30:02.456] TX: Packet seq=42 to gateway (Node 5)
[14:30:03.789] RX: HELLO from Node 3 (Relay), ETX=1.2
[14:30:04.012] [TrickleHELLO] Sending HELLO (I=60.0s)
[14:30:05.345] [Trickle] SUPPRESS - Heard consistent neighbor
[14:30:15.678] [Trickle] DOUBLE - I=120.0s
[14:30:20.901] TX: Sensor data seq=43, PM2.5=15.2 µg/m³
```

### CSV Export Format (Data Analyzer)

Analyzers can export structured data to CSV for Excel/pandas:

```csv
timestamp,node_id,event_type,seq,src,dst,rssi,snr,etx,cost,hop_count,gateway
1699732421123,1,TX,42,1,5,-85,8.5,1.2,1.39,2,5
1699732421456,3,RX,42,1,5,-82,9.2,1.1,1.25,1,5
1699732422789,5,RX,42,1,5,-78,10.1,1.0,1.18,0,5
```

### JSON Output Format (Analysis Results)

The data analyzer produces structured JSON for protocol comparison:

```json
{
  "test_name": "3node_30min_val_10dBm_20251110_182442",
  "protocol": "protocol3",
  "duration_seconds": 1800,
  "metrics": {
    "pdr_percent": 100.0,
    "packets_sent": 119,
    "packets_received": 119,
    "packets_lost": 0,
    "hello_overhead_per_hour": 6.25,
    "trickle_hellos": 7,
    "safety_hellos": 68,
    "suppression_count": 41,
    "suppression_rate": 0.85,
    "avg_latency_ms": 450,
    "median_latency_ms": 380,
    "p95_latency_ms": 720,
    "p99_latency_ms": 890,
    "throughput_bytes_per_hour": 1200,
    "cost_initial": 1.39,
    "cost_final": 1.18,
    "cost_improvement_percent": -15.3
  },
  "topology": {
    "nodes": [1, 3, 5],
    "links": [
      {"src": 1, "dst": 3, "avg_rssi": -85.2, "avg_etx": 1.2},
      {"src": 3, "dst": 5, "avg_rssi": -78.1, "avg_etx": 1.1}
    ]
  }
}
```

### MQTT Topic Structure

The xMESH MQTT integration uses hierarchical topics:

**Sensor Data Topics** (from `mqtt_publisher.py`):

```
mesh/ingest/{node_id}
  ├─ mesh/ingest/BB94        # Sensor node BB94
  ├─ mesh/ingest/C3A1        # Sensor node C3A1
  └─ mesh/ingest/FF02        # Sensor node FF02
```

**Packet Event Topics** (from `serial_collector.py`):

```
xmesh/node/{node_id}/packet
  ├─ xmesh/node/1/packet     # Sensor node 1
  ├─ xmesh/node/3/packet     # Relay node 3
  └─ xmesh/node/5/packet     # Gateway node 5

xmesh/stats                  # Collector statistics
```

**Example MQTT Payload** (Sensor Data):

```json
{
  "timestamp": "2025-11-23T14:32:15.123",
  "sequence": 42,
  "source": "BB94",
  "pm1_0": 12.0,
  "pm2_5": 15.0,
  "pm10": 18.0,
  "latitude": 13.7278,
  "longitude": 100.7753,
  "altitude": 25.5,
  "satellites": 7,
  "gps_valid": true
}
```

**Example MQTT Payload** (Packet Event):

```json
{
  "timestamp": 1699732421123,
  "node_id": 5,
  "event_type": "RX",
  "src": 1,
  "dest": 5,
  "rssi": -85.2,
  "snr": 8.5,
  "etx": 1.2,
  "hop_count": 2,
  "packet_size": 64,
  "sequence": 42,
  "cost": 1.39,
  "next_hop": 3,
  "gateway": 5
}
```

---

## Integration with Firmware Protocols

The Raspberry Pi tools are designed to work with all three xMESH firmware protocols:

### Protocol 1: Flooding (firmware/1_flooding)

- **Capture**: `multi_node_capture.py` records all broadcast packets
- **Analysis**: Measures network-wide flooding overhead
- **Metrics**: Total broadcasts, duplicate packets, network saturation

### Protocol 2: Hop Count Routing (firmware/2_hopcount)

- **Capture**: Tracks HELLO packets every 120 seconds (fixed interval)
- **Analysis**: Calculates PDR, hop count distribution, HELLO overhead
- **Baseline**: Provides comparison baseline for Protocol 3 improvements

### Protocol 3: Gateway Routing with Trickle (firmware/3_gateway_routing)

- **Capture**: Records Trickle events (DOUBLE, SUPPRESS, RESET)
- **MQTT Bridge**: Publishes sensor data (PM2.5, GPS) to cloud
- **Analysis**: Measures overhead reduction, convergence time, load balancing
- **Advanced Metrics**: ETX tracking, cost function evolution, gateway switching

### Serial Output Parsing

The tools parse specific firmware log patterns:

**Protocol 1/2 (Basic)**:

```cpp
// Firmware output
Serial.printf("TX: Packet seq=%d\n", seq);
Serial.printf("RX: Packet seq=%d from %04X\n", seq, src);
```

**Protocol 3 (Enhanced)**:

```cpp
// Firmware output with Trickle and sensor data
Serial.printf("[TrickleHELLO] Sending HELLO (I=%.1fs)\n", interval);
Serial.printf("[Trickle] SUPPRESS - Heard consistent neighbor\n");
Serial.printf("[Trickle] DOUBLE - I=%.1fs\n", new_interval);
Serial.printf("RX: Seq=%d From=%04X\n", seq, src);
Serial.printf("  PM: 1.0=%.1f 2.5=%.1f 10=%.1f µg/m³\n", pm1, pm2_5, pm10);
Serial.printf("  GPS: %.6f°N, %.6f°E, alt=%.1fm, %d sats\n", lat, lon, alt, sats);
```

---

## Configuration Files

### MQTT Configuration (`mqtt_config.json`)

Template configuration for MQTT publisher:

```json
{
  "broker": "mqtt.example.com",
  "port": 8883,
  "username": "your_username",
  "password": "your_password",
  "client_id": "xmesh_gateway_publisher",
  "qos": 1,
  "use_tls": true,
  "tls_insecure": false,
  "ca_cert": "/path/to/ca-certificate.crt",
  "topics": {
    "sensor_data": "mesh/ingest/{node_id}"
  },
  "keepalive": 60,
  "clean_session": true
}
```

**Configuration Parameters**:

- `broker`: MQTT broker hostname or IP address
- `port`: MQTT port (1883 for plain, 8883 for TLS)
- `username`, `password`: Authentication credentials
- `client_id`: Unique identifier for this client
- `qos`: Quality of Service (0=at most once, 1=at least once, 2=exactly once)
- `use_tls`: Enable SSL/TLS encryption
- `tls_insecure`: Skip certificate verification (not recommended for production)
- `ca_cert`: Path to CA certificate bundle (defaults to system certifi)
- `topics.sensor_data`: Topic pattern with `{node_id}` placeholder
- `keepalive`: Connection keepalive interval (seconds)
- `clean_session`: Clear session state on connect

### Local MQTT Broker Setup (Optional)

For testing without cloud infrastructure:

```bash
# Install Mosquitto MQTT broker
sudo apt-get install mosquitto mosquitto-clients

# Start broker
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

# Test with mosquitto_sub
mosquitto_sub -h localhost -t "mesh/ingest/#" -v

# Configure xMESH to use local broker
{
  "broker": "localhost",
  "port": 1883,
  "username": "",
  "password": "",
  "use_tls": false
}
```

---

## Python Dependencies Reference

Install required Python packages:

```bash
pip3 install pyserial pandas numpy matplotlib seaborn scipy
```

Or use requirements.txt (if available):

```bash
pip3 install -r raspberry_pi/requirements.txt
```

---

## Troubleshooting Guide

### Serial Port Issues

#### Issue: "Permission denied" on serial port

**macOS**:
```bash
# Add user to dialout group (may require logout)
sudo dseditgroup -o edit -a $USER -t user dialout

# Or use sudo (not recommended for long tests)
sudo python3 raspberry_pi/multi_node_capture.py ...
```

**Linux**:
```bash
sudo usermod -a -G dialout $USER
# Logout and login
```

#### Issue: Port not found

```bash
# List available ports
ls /dev/cu.* # macOS
ls /dev/ttyUSB* # Linux
ls /dev/ttyACM* # Linux (alternate)

# Python serial port list
python3 -m serial.tools.list_ports

# Check dmesg for connection events
dmesg | grep tty  # Linux
```

**Solution**: Verify USB connection, try different cable/port, check ESP32 is powered on.

#### Issue: "Device busy" or port already in use

```bash
# Find process using the port
lsof | grep ttyUSB0  # Linux
lsof | grep cu.usbserial  # macOS

# Kill the process
kill -9 <PID>
```

**Solution**: Close Arduino Serial Monitor, other capture scripts, or PlatformIO monitor.

#### Issue: Garbled serial output

**Symptoms**: Random characters, unreadable text

**Solutions**:

- Verify baud rate matches firmware (default: 115200)
- Check USB cable quality (use short, high-quality cables)
- Try different USB port (avoid USB 2.0 hubs with USB 3.0 devices)
- Add delay after serial connection: `time.sleep(2)` before reading

#### Issue: Intermittent disconnections

**Solutions**:

- Use powered USB hub (don't draw power from Pi GPIO)
- Check Pi power supply (5V/3A minimum)
- Disable USB autosuspend on Linux:

```bash
# Disable USB autosuspend temporarily
echo -1 | sudo tee /sys/module/usbcore/parameters/autosuspend

# Or permanently in /etc/rc.local
echo 'echo -1 > /sys/module/usbcore/parameters/autosuspend' | sudo tee -a /etc/rc.local
```

### MQTT Connection Issues

#### Issue: "MQTT connection failed with code 5"

**Code 5 = Authentication failed**

**Solutions**:

- Verify username/password in `mqtt_config.json`
- Check broker allows remote connections (not just localhost)
- Ensure credentials have publish permissions for the topic

#### Issue: "SSL handshake failed"

**Solutions**:

```bash
# Update certifi for latest CA certificates
pip3 install --upgrade certifi

# Check broker certificate validity
openssl s_client -connect mqtt.example.com:8883 -showcerts

# For testing only, enable tls_insecure (NOT for production)
{
  "use_tls": true,
  "tls_insecure": true
}
```

#### Issue: MQTT messages not arriving at broker

**Debug steps**:

```bash
# Test MQTT broker with mosquitto_pub
mosquitto_pub -h mqtt.example.com -p 1883 -t "test/topic" -m "hello"

# Subscribe to all topics
mosquitto_sub -h mqtt.example.com -p 1883 -t "#" -v

# Check xMESH publisher output for error messages
python3 raspberry_pi/mqtt_publisher.py --port /dev/ttyUSB0 --config mqtt_config.json
```

### Data Collection Issues

#### Issue: Missing data in analysis

**Check test health first**:

```bash
python3 raspberry_pi/test_health_check.py experiments/results/protocol3/test_XXXXX/
```

**Common causes**:

- Node disconnected during test (check USB cables)
- USB buffer overflow (reduce capture duration, increase USB buffer size)
- Power supply brownout (use adequate power supply)
- SD card write errors (check `dmesg` for I/O errors)

**Solutions**:

```bash
# Increase USB buffer size (Linux)
echo 4096 | sudo tee /sys/module/usbserial/parameters/buffer_size

# Monitor system resources during capture
htop
iostat -x 1

# Use SSD instead of SD card for intensive logging
```

#### Issue: Timestamp drift between nodes

**Symptoms**: Logs show events out of order

**Explanation**: Each node log uses Raspberry Pi timestamp, not ESP32 internal time.

**Solution**: Use `multi_node_capture.py` which synchronizes all captures to start simultaneously.

#### Issue: Log files incomplete or corrupted

**Solutions**:

- Check disk space: `df -h`
- Verify write permissions: `ls -la experiments/results/`
- Test SD card for errors: `sudo badblocks -v /dev/mmcblk0`
- Use `fsync()` or disable write caching for critical tests

### Analysis and Plotting Issues

#### Issue: Plots not generating

**Install matplotlib backend**:

```bash
# macOS
pip3 install pyqt5

# Linux (headless Raspberry Pi)
export MPLBACKEND=Agg
pip3 install matplotlib

# Add to .bashrc for persistence
echo 'export MPLBACKEND=Agg' >> ~/.bashrc
```

#### Issue: "No module named pandas/numpy/matplotlib"

**Solution**:

```bash
# Install all dependencies
pip3 install -r raspberry_pi/requirements.txt

# Or install individually
pip3 install pandas numpy matplotlib scipy seaborn
```

#### Issue: Data analyzer shows 0% PDR but packets were received

**Cause**: Log format mismatch or parsing error

**Debug**:

```bash
# Check log file format
head -20 experiments/results/protocol3/test_*/node*.log

# Verify TX/RX lines are present
grep "TX:" experiments/results/protocol3/test_*/node1.log | wc -l
grep "RX:" experiments/results/protocol3/test_*/node5.log | wc -l
```

**Solution**: Ensure firmware uses correct printf format (see "Integration with Firmware" section).

### Performance Issues

#### Issue: Raspberry Pi runs out of memory during long tests

**Solutions**:

```bash
# Increase swap size
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile  # Set CONF_SWAPSIZE=2048
sudo dphys-swapfile setup
sudo dphys-swapfile swapon

# Monitor memory usage
free -h
watch -n 1 free -h
```

#### Issue: CPU usage at 100% during multi-node capture

**Expected**: Python serial processing is CPU-intensive

**Solutions**:

- Use Raspberry Pi 4 (quad-core)
- Reduce number of simultaneous nodes
- Lower baud rate (57600 instead of 115200)
- Close unnecessary background processes

### Firmware Integration Issues

#### Issue: MQTT publisher not parsing sensor data

**Cause**: Firmware output format mismatch

**Verify firmware output**:

```bash
# Connect to gateway with screen
screen /dev/ttyUSB0 115200

# Expected output for Protocol 3:
# RX: Seq=1 From=BB94
#   PM: 1.0=12 2.5=15 10=18 µg/m³
#   GPS: 13.727800°N, 100.775300°E, alt=25.5m, 7 sats
```

**Solution**: Update firmware to match expected format in `mqtt_publisher.py` regex patterns.

#### Issue: Trickle metrics not detected by multi_node_capture

**Verify firmware logs Trickle events**:

```bash
# Check for Trickle debug output
grep "Trickle" experiments/results/protocol3/test_*/node*.log
```

**Expected**:

```
[TrickleHELLO] Sending HELLO (I=60.0s)
[Trickle] SUPPRESS - Heard consistent neighbor
[Trickle] DOUBLE - I=120.0s
[Trickle] RESET - Topology change detected
```

**Solution**: Enable Trickle debug logging in firmware `src/main.cpp`.

---

---

## Best Practices

### For Experiments

1. **Label Everything**: Physically label USB cables and nodes (1, 2, 3, etc.)
2. **Test Before Long Runs**: Do a 1-minute capture to verify all connections
3. **Use Timestamped Folders**: Always use `$(date +%Y%m%d_%H%M%S)` in output paths
4. **Check Logs Immediately**: Verify log files have content before ending experiment
5. **Document Topology**: Take photos of node placement for outdoor tests
6. **Monitor Resources**: Check disk space and memory before long tests

### For Production Deployment

1. **Use Systemd Services**: Run MQTT publisher as a service for reliability
2. **Enable Log Rotation**: Prevent disk space exhaustion
3. **Monitor MQTT Connection**: Set up alerts for disconnections
4. **Use SSD Storage**: SD cards fail over time; use USB SSD for critical data
5. **Regular Backups**: Backup experiment data to cloud/network storage
6. **Version Control**: Tag firmware versions used for each experiment

### For Data Analysis

1. **Run Health Check First**: Always validate data before analysis
2. **Save Raw Logs**: Never delete original log files
3. **Export to Multiple Formats**: CSV for Excel, JSON for code, plots for papers
4. **Document Parameters**: Record test conditions in experiment metadata
5. **Compare Protocols**: Use same topology/duration for fair comparison

---

## Common Use Cases

### Use Case 1: Indoor Lab Testing (3 Nodes, 30 Minutes)

Goal: Validate protocol changes, measure PDR and overhead

```bash
# Setup
cd /home/pi/xMESH-1
TEST_DIR="experiments/results/protocol3/indoor_$(date +%Y%m%d_%H%M%S)"

# Capture
python3 raspberry_pi/multi_node_capture.py \
  --nodes 1,3,5 \
  --ports /dev/ttyUSB0,/dev/ttyUSB1,/dev/ttyUSB2 \
  --duration 1800 \
  --output "$TEST_DIR"

# Analyze
python3 raspberry_pi/data_analyzer.py "$TEST_DIR"/*.log --plot --report
```

### Use Case 2: Outdoor Deployment (2 Gateway Nodes, 24 Hours)

Goal: Long-term monitoring with MQTT streaming

```bash
# Terminal 1: Gateway 1 (Node 4)
python3 raspberry_pi/mqtt_publisher.py \
  --port /dev/ttyUSB0 \
  --config mqtt_config.json \
  > /var/log/xmesh/gateway4_$(date +%Y%m%d).log 2>&1 &

# Terminal 2: Gateway 2 (Node 5)
python3 raspberry_pi/mqtt_publisher.py \
  --port /dev/ttyUSB1 \
  --config mqtt_config.json \
  > /var/log/xmesh/gateway5_$(date +%Y%m%d).log 2>&1 &

# Monitor MQTT traffic
mosquitto_sub -h mqtt.ait.ac.th -p 8883 \
  -u username -P password \
  -t "mesh/ingest/#" -v
```

### Use Case 3: Protocol Comparison (All 3 Protocols, Same Topology)

Goal: Compare Protocol 1, 2, 3 performance

```bash
# Flash and test each protocol
for PROTOCOL in 1 2 3; do
  # Flash firmware
  cd firmware/${PROTOCOL}_*
  ./flash_all_nodes.sh
  cd ../..

  # Wait for nodes to boot
  sleep 10

  # Capture 30 minutes
  TEST_DIR="experiments/results/protocol${PROTOCOL}/comparison_$(date +%Y%m%d_%H%M%S)"
  python3 raspberry_pi/multi_node_capture.py \
    --nodes 1,3,5 \
    --ports /dev/ttyUSB0,/dev/ttyUSB1,/dev/ttyUSB2 \
    --duration 1800 \
    --output "$TEST_DIR"

  # Cool-down period
  sleep 60
done

# Compare results
python3 raspberry_pi/data_analyzer.py \
  experiments/results/protocol*/comparison_*/node*.log \
  --compare --output protocol_comparison.json --plot
```

---

## Related Documentation

For complete xMESH documentation:

- [Main README](../README.md) - Project overview and quick start
- [Firmware Guide](../firmware/README.md) - Build and flash ESP32 firmware
- Protocol Documentation:
  - [Protocol 1: Flooding](../firmware/1_flooding/README.md)
  - [Protocol 2: Hop Count Routing](../firmware/2_hopcount/README.md)
  - [Protocol 3: Gateway Routing with Trickle](../firmware/3_gateway_routing/README.md)

---

### Citing This Work

If you use xMESH in academic research, please cite:

```bibtex
@misc{xmesh2025,
  title={Design and Implementation of a LoRa Mesh Network with an Optimized Routing Protocol},
  author={Nyein Chan Win Naing},
  year={2025},
  institution={Asian Institute of Technology},
  howpublished={\url{https://github.com/ncwn/xMESH}}
}
```

---

**Last Updated**: November 2025
**Maintainer**: Nyein Chan Win Naing
**Version**: 1.0.0
