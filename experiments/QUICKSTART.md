# Week 6-7 Quick Start Guide

## Performance Evaluation Setup

### Prerequisites
- 3x Heltec WiFi LoRa32 V3 nodes (charged)
- USB cable for gateway monitoring
- Python 3.8+ installed
- PlatformIO CLI

### Step 1: Install Analysis Tools

```bash
# Install Python dependencies
cd analysis
pip install -r requirements.txt
```

### Step 2: Prepare Test Environment

1. **Measure distances:** Mark 5m, 10m positions
2. **Label nodes:** Sensor (BB94), Router (6674), Gateway (D218)
3. **Test topology:** Linear chain (Sensor → Router → Gateway)

### Step 3: Test Each Protocol

#### A. Flooding Baseline (v0.2.0-alpha)

```bash
# 1. Flash firmware
cd firmware/1_flooding
pio run -e sensor -t upload    # Flash sensor node
pio run -e router -t upload    # Flash router node
pio run -e gateway -t upload   # Flash gateway node

# 2. Deploy nodes at marked distances

# 3. Start data collection (30 minutes)
cd ../../
python utilities/data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --output experiments/results/flooding/run1.csv

# 4. Repeat for run2 and run3
```

#### B. Hop-Count Routing (v0.3.1-alpha)

```bash
# 1. Flash firmware
cd firmware/2_hopcount
pio run -e sensor -t upload
pio run -e router -t upload
pio run -e gateway -t upload

# 2. Deploy nodes (same topology)

# 3. Collect data
cd ../../
python utilities/data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --output experiments/results/hopcount/run1.csv

# 4. Repeat for run2 and run3
```

#### C. Cost Routing (v0.4.0-alpha)

```bash
# 1. Flash firmware
cd firmware/3_gateway_routing
pio run -e sensor -t upload
pio run -e router -t upload
pio run -e gateway -t upload

# 2. Deploy nodes (same topology)

# 3. Collect data
cd ../../
python utilities/data_collection.py \
  --port /dev/cu.usbserial-0001 \
  --output experiments/results/cost_routing/run1.csv

# 4. Repeat for run2 and run3
```

### Step 4: Analyze Results

```bash
# Generate analysis report
python analysis/performance_analysis.py --data-dir experiments/results

# Check outputs
ls experiments/results/figures/
# - pdr_comparison.png
# - cost_evolution.png
```

### Step 5: Review Results

1. Check PDR for all protocols (target: ≥95%)
2. Compare link quality metrics
3. Review cost evolution (cost routing only)
4. Document observations in SUMMARY.md

## Expected Results Directory Structure

```
experiments/
  results/
    flooding/
      run1.csv
      run2.csv
      run3.csv
    hopcount/
      run1.csv
      run2.csv
      run3.csv
    cost_routing/
      run1.csv
      run2.csv
      run3.csv
    figures/
      pdr_comparison.png
      cost_evolution.png
    SUMMARY.md
```

## Troubleshooting

### Nodes not communicating
- Check TX power (10 dBm for testing)
- Verify LoRa frequency (915 MHz)
- Reduce distance to 3-5m
- Check serial output for errors

### Data collection script errors
- Verify correct serial port: `ls /dev/cu.*` (macOS) or `ls /dev/ttyUSB*` (Linux)
- Check permissions: `sudo chmod 666 /dev/ttyUSB0`
- Test baudrate: Try 9600 if 115200 fails

### Missing packets
- Extend test duration to 45-60 minutes
- Check for environmental interference
- Verify sensor is transmitting (check LED/display)

### Analysis script fails
- Ensure CSV files exist in correct directories
- Check CSV format matches expected schema
- Verify all dependencies installed: `pip list | grep pandas`

## Tips for Good Data

1. **Consistent topology:** Use same node positions for all tests
2. **Clean environment:** Test away from WiFi/Bluetooth devices
3. **Fresh start:** Power cycle nodes between runs
4. **Monitor actively:** Watch for crashes or anomalies
5. **Document everything:** Note any unusual behavior

## Time Budget

- Flooding tests: 3 runs × 30 min = 90 minutes
- Hop-count tests: 3 runs × 30 min = 90 minutes
- Cost routing tests: 3 runs × 30 min = 90 minutes
- Analysis and documentation: 120 minutes
- **Total: ~6.5 hours** (can be done over 2-3 days)

## Next Steps After Testing

1. Run statistical analysis
2. Create comparison tables
3. Generate visualization plots
4. Write discussion of findings
5. Update CHANGELOG with Week 6-7 entry
6. Commit and tag results

---

**Status:** Ready for testing
**Protocol:** experiments/configs/week6_7_protocol.md
**Tools:** data_collection.py, performance_analysis.py
