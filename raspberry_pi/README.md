# xMESH Data Collection & Analysis Tools

Python utilities for capturing serial data from multiple LoRa nodes and analyzing experimental results.

## Tools Overview

| Script | Purpose | Usage |
|--------|---------|-------|
| `multi_node_capture.py` | Capture logs from multiple nodes simultaneously | Testing & data collection |
| `data_analyzer.py` | Statistical analysis of test results | Post-processing |
| `serial_collector.py` | Single-node data capture | Individual node debugging |
| `test_health_check.py` | Validate test data completeness | Quality assurance |

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

Single-node serial capture for debugging individual nodes.

### Usage

```bash
# Capture from single node
python3 raspberry_pi/serial_collector.py \
  --port /dev/cu.usbserial-0001 \
  --baudrate 115200 \
  --output debug_node1.log \
  --duration 600
```

### When to Use

- Debugging individual node behavior
- Monitoring single gateway during development
- Quick verification before multi-node tests

---

## 4. Test Health Check (`test_health_check.py`)

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

## Dependencies

Install required Python packages:

```bash
pip3 install pyserial pandas numpy matplotlib seaborn scipy
```

Or use requirements.txt (if available):

```bash
pip3 install -r raspberry_pi/requirements.txt
```

---

## Troubleshooting

### Issue: "Permission denied" on serial port

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

### Issue: Port not found

```bash
# List available ports
ls /dev/cu.* # macOS
ls /dev/ttyUSB* # Linux

# Or use Arduino IDE port detection
```

### Issue: Missing data in analysis

Check test health first:
```bash
python3 raspberry_pi/test_health_check.py experiments/results/protocol3/test_XXXXX/
```

Common causes:
- Node disconnected during test
- USB cable loose
- Buffer overflow (reduce baud rate or test duration)
- Power supply issue

### Issue: Plots not generating

Install matplotlib backend:
```bash
# macOS
pip3 install pyqt5

# Linux (headless)
export MPLBACKEND=Agg
```

---

## File Formats

### Log File Format

Each node log contains timestamped serial output:
```
[2025-11-10 15:23:41.123] xMESH Node 1 (Sensor) - Protocol 3
[2025-11-10 15:23:42.456] TX: Packet seq=42 to gateway (Node 5)
[2025-11-10 15:23:43.789] RX: HELLO from Node 3 (Relay)
...
```

### CSV Export Format

Analyzers can export to CSV:
```csv
timestamp,node_id,event_type,seq,src,dst,rssi,snr,etx,cost
1699732421123,1,TX,42,1,5,-85,8.5,1.2,1.39
1699732421456,3,RX,42,1,5,-82,9.2,1.1,1.25
...
```

---

## Contributing

When adding new analysis scripts:
1. Follow existing naming conventions
2. Add argparse help text for all parameters
3. Include example usage in docstring
4. Update this README with new tool description

---

## Related Documentation

- [EXPERIMENT_PROTOCOL.md](../docs/EXPERIMENT_PROTOCOL.md) - Testing methodology
- [PROTOCOL3_COMPLETE.md](../firmware/3_gateway_routing/PROTOCOL3_COMPLETE.md) - Protocol details
- [FIRMWARE_GUIDE.md](../docs/FIRMWARE_GUIDE.md) - Build and flash procedures
