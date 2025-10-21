# 🚀 QUICK START: Hardware Testing (TL;DR)

**Current Date**: October 21, 2025  
**Your Status**: Monitoring code complete ✅, Ready to test!  
**Time Needed**: ~2-3 hours per protocol

---

## ⚡ Super Quick Version (If You Know What You're Doing)

### 1. Flash Flooding Firmware (5 min)
```bash
cd /Volumes/Data/xMESH/xLAB/xMESH/firmware/1_flooding

# Flash each node
pio run -e sensor --target upload    # BB94
pio run -e router --target upload    # 6674
pio run -e gateway --target upload   # D218
```

### 2. Find Serial Ports (1 min)
```bash
pio device list
# Note the 3 ports: /dev/cu.usbserial-XXXXX (BB94, 6674, D218)
```

### 3. Run Tests (90 min = 3 runs × 30 min)
```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

# Run 1
python3 utilities/multi_node_data_collection.py \
  --protocol flooding \
  --run-number 1 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX \
  --router-port /dev/cu.usbserial-YYYYY \
  --gateway-port /dev/cu.usbserial-ZZZZZ \
  --output-dir experiments/results

# Run 2 (wait 2 min, then repeat with --run-number 2)
# Run 3 (wait 2 min, then repeat with --run-number 3)
```

### 4. Repeat for Other Protocols
- Hop-count: `cd firmware/2_hopcount`, flash, test
- Cost routing: `cd firmware/3_gateway_routing`, flash, test

**Done!** You'll have 27 CSV files ready for Week 2 analysis.

---

## 📖 Detailed Steps (If First Time)

### Step 1: Prepare Hardware
1. **Charge all 3 Heltec nodes** (USB-C)
2. **Label them**: BB94 (sensor), 6674 (router), D218 (gateway)
3. **Place nodes** 5 meters apart in line: BB94 ↔ 6674 ↔ D218
4. **Connect all 3 to MacBook** via USB-C (use hub if needed)

### Step 2: Flash Flooding Firmware
```bash
cd /Volumes/Data/xMESH/xLAB/xMESH/firmware/1_flooding

# Connect BB94 first
pio run -e sensor --target upload
# Wait for "SUCCESS"

# Connect 6674
pio run -e router --target upload

# Connect D218
pio run -e gateway --target upload
```

**Verify Each Node:**
```bash
# Check BB94 shows "Role: SENSOR"
pio device monitor --baud 115200
# Press Ctrl+C to exit

# Check 6674 shows "Role: ROUTER"
# Check D218 shows "Role: GATEWAY"
```

### Step 3: Identify Serial Ports
```bash
# List all connected devices
pio device list

# You'll see 3 ports like:
# /dev/cu.usbserial-0001
# /dev/cu.usbserial-0002  
# /dev/cu.usbserial-0003

# To identify which is which:
screen /dev/cu.usbserial-0001 115200
# Look for "Local address: BB94" (or 6674, D218)
# Press Ctrl+A then K to exit

# Repeat for each port to map node IDs to ports
```

### Step 4: Test Manual Monitoring (Optional, 2 min)
Open 3 terminals to verify nodes communicate:

**Terminal 1:**
```bash
screen /dev/cu.usbserial-XXXXX 115200  # Sensor BB94
```

**Terminal 2:**
```bash
screen /dev/cu.usbserial-YYYYY 115200  # Router 6674
```

**Terminal 3:**
```bash
screen /dev/cu.usbserial-ZZZZZ 115200  # Gateway D218
```

**What you should see every 60 seconds:**
- **Sensor**: `TX: Seq=1 Value=XX.XX`
- **Router**: `RX: Seq=1 From=BB94` then `FLOOD: Rebroadcasting`
- **Gateway**: `RX: Seq=1 From=BB94 Hops=2` then `GATEWAY: Packet received`

**Every 30 seconds, all nodes print:**
```
==== Network Monitoring Stats ====
Channel: 0.XXX% duty-cycle, N TX, 0 violations
Memory: XXX/320 KB free, Min: XXX KB, Peak: XX KB
Queue: N enqueued, 0 dropped (0.00%), max depth: 1
====================================
```

✅ **If you see this, nodes are working!**  
❌ **If not, check distance (try 2-3m), antennas, or reflash firmware**

Exit all terminals: Press `Ctrl+A` then `K` in each.

### Step 5: Run Automated Data Collection

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

# Create results directories
mkdir -p experiments/results/flooding
mkdir -p experiments/results/hopcount
mkdir -p experiments/results/cost_routing

# Run flooding test 1 (30 minutes)
python3 utilities/multi_node_data_collection.py \
  --protocol flooding \
  --run-number 1 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX \
  --router-port /dev/cu.usbserial-YYYYY \
  --gateway-port /dev/cu.usbserial-ZZZZZ \
  --output-dir experiments/results
```

**Script will show:**
```
=== LoRa Mesh Network Data Collection ===
Protocol: flooding
Run: 1
Duration: 30 minutes

Connecting to nodes...
✓ SENSOR (BB94): /dev/cu.usbserial-XXXXX
✓ ROUTER (6674): /dev/cu.usbserial-YYYYY
✓ GATEWAY (D218): /dev/cu.usbserial-ZZZZZ

Data collection started at 14:30:00
Progress: [=====>            ] 5/30 min (16%)
Progress: [===========>      ] 10/30 min (33%)
...
Progress: [====================] 30/30 min (100%)

Data collection complete!

=== Collection Summary ===
SENSOR (BB94):
  Monitoring updates: 60
  Max duty-cycle: 0.150%
  Min free memory: 285 KB
  ...

Files saved:
  - experiments/results/flooding/run1_BB94_sensor.csv
  - experiments/results/flooding/run1_6674_router.csv
  - experiments/results/flooding/run1_D218_gateway.csv
```

### Step 6: Run Remaining Tests

**Flooding Runs 2 & 3:**
```bash
# Wait 2 minutes between runs for nodes to stabilize

# Run 2
python3 utilities/multi_node_data_collection.py \
  --protocol flooding \
  --run-number 2 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX \
  --router-port /dev/cu.usbserial-YYYYY \
  --gateway-port /dev/cu.usbserial-ZZZZZ \
  --output-dir experiments/results

# Run 3 (repeat with --run-number 3)
```

### Step 7: Flash and Test Hop-Count
```bash
cd firmware/2_hopcount

# Flash all 3 nodes
pio run -e sensor --target upload
pio run -e router --target upload
pio run -e gateway --target upload

# Wait 2-3 minutes for routing tables to build

# Run tests (repeat Step 5-6 with --protocol hopcount)
```

### Step 8: Flash and Test Cost Routing
```bash
cd firmware/3_gateway_routing

# Flash all 3 nodes
pio run -e sensor --target upload
pio run -e router --target upload
pio run -e gateway --target upload

# Wait 2-3 minutes for routing tables to build

# Run tests (repeat Step 5-6 with --protocol cost_routing)
```

---

## ✅ Success Checklist

After all tests, verify:

```bash
# Check all 27 files exist
ls experiments/results/flooding/
ls experiments/results/hopcount/
ls experiments/results/cost_routing/

# Should see 9 files in each directory:
# run1_BB94_sensor.csv
# run1_6674_router.csv
# run1_D218_gateway.csv
# run2_... (same pattern)
# run3_... (same pattern)

# Quick file size check
ls -lh experiments/results/*/*.csv

# Each file should be 20-100 KB
# If any file is <10 KB, that run may have failed
```

---

## 🐛 Troubleshooting

### Problem: "Port not found" error
**Solution:**
```bash
# List available ports
ls /dev/cu.*

# Use the actual port names shown
```

### Problem: Script crashes with "Permission denied"
**Solution:**
```bash
# Add execute permission
chmod +x utilities/multi_node_data_collection.py

# Install required Python packages
pip3 install pyserial
```

### Problem: Nodes don't communicate
**Solution:**
1. Reduce distance to 2-3 meters
2. Check antennas are connected and vertical
3. Disable WiFi/Bluetooth on MacBook (interference)
4. Reset all nodes (RST button) and retry

### Problem: High packet loss (PDR <80%)
**Solution:**
1. Move nodes closer
2. Test each link individually
3. Check for metal/water obstacles
4. Try different physical location

### Problem: Duty-cycle >1%
**Solution:**
- This is expected for flooding at larger scales
- For 3 nodes, should be <0.5%
- If >1% at 3 nodes, check for packet loops or retransmission issues

---

## 📊 What You'll Have After Week 1

```
experiments/results/
├── flooding/
│   ├── run1_BB94_sensor.csv (monitoring data from BB94)
│   ├── run1_6674_router.csv (monitoring data from 6674)
│   ├── run1_D218_gateway.csv (monitoring data from D218)
│   ├── run2_... (9 files total)
│   └── run3_...
├── hopcount/
│   └── (9 files, same structure)
└── cost_routing/
    └── (9 files, same structure)
```

**CSV Format:**
```csv
timestamp,node_id,role,duty_cycle_pct,tx_count,violations,memory_free_kb,...
2025-10-21T14:30:15,BB94,SENSOR,0.125,1,0,285,320,280,35,1,0,0.0,1,0,...
2025-10-21T14:30:45,BB94,SENSOR,0.150,1,0,283,320,280,37,1,0,0.0,1,0,...
```

---

## 🎯 Next: Week 2 Analytical Modeling

Once you have all 27 CSV files:

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

# Run scalability analysis
python3 analysis/scalability_model.py \
  --data-dir experiments/results \
  --output-dir experiments/results/figures

# Generates plots showing:
# - Flooding breaks at ~15 nodes (duty-cycle >1%)
# - Hop-count breaks at ~40 nodes
# - Cost routing scales to ~50 nodes
```

---

## 📝 Test Log Template

Keep notes during testing:

```
Date: October 21, 2025
Protocol: Flooding
Run: 1
Start Time: 14:30
End Time: 15:00
Weather: Indoor, 25°C
Distance: 5m between nodes
Issues: None
PDR: ~95% (estimated from gateway logs)
Max Duty-Cycle: 0.18%
Notes: All nodes stable, no crashes
```

---

## ⏱️ Time Estimates

| Task | Time | Notes |
|------|------|-------|
| Setup hardware | 10 min | One-time |
| Flash firmware | 5 min | Per protocol |
| Identify ports | 2 min | Per protocol |
| Run 1 test | 30 min | Actual test time |
| Between-run wait | 2 min | Stabilization |
| **Per protocol total** | **~2 hours** | 3 runs × 30 min + overhead |
| **All 3 protocols** | **~6 hours** | Spread over 2-3 days |

**Realistic Schedule:**
- **Day 1 (Oct 21)**: Flooding (2 hours)
- **Day 2 (Oct 22)**: Hop-count (2 hours)
- **Day 3 (Oct 23)**: Cost routing (2 hours)
- **Day 4 (Oct 24)**: Buffer/re-runs if needed

---

## 🚨 When to Ask for Help

✅ **You can handle:**
- Port connection issues (check `pio device list`)
- Node won't flash (hold BOOT button)
- Script crashes (check Python version, permissions)

❌ **Contact advisor if:**
- All 3 nodes can't communicate after 1 hour troubleshooting
- Consistent crashes/reboots (hardware failure?)
- Data looks completely wrong (duty-cycle >10%, memory dropping to 0)
- Major schedule impact (can't complete Week 1)

---

**Ready to start?** Begin with flooding protocol! 🎉

See `experiments/testing_instructions.md` for detailed step-by-step walkthrough.
