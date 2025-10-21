# Step-by-Step Hardware Testing Instructions

**Date**: October 21, 2025  
**Estimated Time**: 2-3 hours per protocol (including setup)  
**Current Protocol**: Start with Flooding (v0.2.0-alpha)

---

## 🚀 Quick Start (Do This First!)

### Step 0: Verify Everything is Ready

```bash
# Navigate to project directory
cd /Volumes/Data/xMESH/xLAB/xMESH

# Check PlatformIO works
pio --version
# Should show: PlatformIO Core, version 6.x.x

# Check Python setup
python3 --version
# Should show: Python 3.8+ 

# Check data collection script exists
ls -l utilities/data_collection.py
# Should show: -rwxr-xr-x ... data_collection.py

# Create results directories
mkdir -p experiments/results/flooding
mkdir -p experiments/results/hopcount
mkdir -p experiments/results/cost_routing
```

---

## 📱 PROTOCOL 1: FLOODING (Start Here!)

### Step 1: Flash Flooding Firmware to All 3 Nodes

**Node 1: BB94 (Sensor)**
```bash
cd firmware/1_flooding

# Connect BB94 node via USB-C
# Check it appears:
pio device list
# Should show: /dev/cu.usbserial-XXXXXX

# Flash sensor firmware
pio run -e sensor --target upload

# Open serial monitor to verify (Ctrl+C to exit)
pio device monitor --baud 115200
# Should see: "xMESH Flooding Protocol"
#             "Role: SENSOR"
#             "Local address: BB94"
```

**Node 2: 6674 (Router)**
```bash
# Disconnect BB94, connect 6674 node
pio run -e router --target upload

# Verify with serial monitor
pio device monitor --baud 115200
# Should see: "Role: ROUTER"
#             "Local address: 6674"
```

**Node 3: D218 (Gateway)**
```bash
# Disconnect 6674, connect D218 node
pio run -e gateway --target upload

# Verify with serial monitor
pio device monitor --baud 115200
# Should see: "Role: GATEWAY"
#             "Local address: D218"
```

---

### Step 2: Physical Node Placement

```
Layout (Top view):

[SENSOR BB94] ←--5m--→ [ROUTER 6674] ←--5m--→ [GATEWAY D218]
     USB                    USB                      USB
      ↓                      ↓                        ↓
  [MacBook Terminal 1]  [Terminal 2]           [Terminal 3]
```

**Placement Tips:**
- Use clear line of sight (no walls/obstacles)
- Maintain 5m distance (use measuring tape or count steps)
- Place nodes on stable surface (table/desk)
- Keep away from WiFi routers, microwaves
- Orient antennas vertically (parallel to each other)

---

### Step 3: Connect All Nodes to MacBook

You need **3 USB ports** (use hub if needed).

```bash
# Check all 3 nodes are connected
pio device list

# Should show 3 serial ports:
# /dev/cu.usbserial-XXXXX1  (BB94 - Sensor)
# /dev/cu.usbserial-XXXXX2  (6674 - Router)
# /dev/cu.usbserial-XXXXX3  (D218 - Gateway)
```

**Identify which port is which node:**
```bash
# Open each port and check "Local address" line
screen /dev/cu.usbserial-XXXXX1 115200
# Press Ctrl+A then K to exit

# Repeat for other ports to find BB94, 6674, D218
```

---

### Step 4: Test Manual Monitoring (1 minute warmup)

Open **3 terminal windows** side-by-side:

**Terminal 1 (Sensor BB94):**
```bash
screen /dev/cu.usbserial-XXXXX1 115200
```

**Terminal 2 (Router 6674):**
```bash
screen /dev/cu.usbserial-XXXXX2 115200
```

**Terminal 3 (Gateway D218):**
```bash
screen /dev/cu.usbserial-XXXXX3 115200
```

**What You Should See:**

**Sensor (Terminal 1)** - Every 60 seconds:
```
TX: Seq=1 Value=45.32
==== Network Monitoring Stats ====
Channel: 0.XXX% duty-cycle, 1 TX, 0 violations
Memory: 290/320 KB free, Min: 285 KB, Peak: 30 KB
Queue: 1 enqueued, 0 dropped (0.00%), max depth: 1
Duplicate cache: 5 entries × 6 bytes = 30 bytes
====================================
```

**Router (Terminal 2)** - Forwarding:
```
RX: Seq=1 From=BB94 Hops=1 Value=45.32
FLOOD: Rebroadcasting packet 1 from BB94
==== Network Monitoring Stats ====
Channel: 0.XXX% duty-cycle, 1 TX, 0 violations
Memory: 288/320 KB free, Min: 283 KB, Peak: 32 KB
Queue: 1 enqueued, 0 dropped (0.00%), max depth: 1
Duplicate cache: 5 entries × 6 bytes = 30 bytes
====================================
```

**Gateway (Terminal 3)** - Receiving:
```
RX: Seq=1 From=BB94 Hops=2 Value=45.32
GATEWAY: Packet 1 from BB94 received (hops=2, value=45.32)
==== Network Monitoring Stats ====
Channel: 0.000% duty-cycle, 0 TX, 0 violations
Memory: 292/320 KB free, Min: 290 KB, Peak: 28 KB
Queue: 0 enqueued, 0 dropped (0.00%), max depth: 0
Duplicate cache: 5 entries × 6 bytes = 30 bytes
====================================
```

**✅ Success indicators:**
- Sensor sends packet every 60s
- Router receives and rebroadcasts
- Gateway receives with hops=2
- Monitoring stats print every 30s
- No error messages

**❌ If you see problems:**
- "No RX packets" → Reduce distance to 2-3m
- "ERROR: Null packet" → Reset nodes (RST button)
- Blank terminal → Check baud rate (115200)

---

### Step 5: Run First Test with Data Collection

**Exit all screen sessions** (Ctrl+A then K in each terminal).

Now run the **automated data collection script**:

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

# Run data collection for 30 minutes (flooding, run 1)
python3 utilities/data_collection.py \
  --protocol flooding \
  --run-number 1 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results
```

**Script will:**
1. Connect to all 3 serial ports
2. Log data from all nodes simultaneously
3. Print progress every 5 minutes
4. Save CSV files when complete (30 min later)

**Expected output:**
```
=== LoRa Mesh Network Data Collection ===
Protocol: flooding
Run: 1
Duration: 30 minutes

Connecting to nodes...
✓ Sensor (BB94): /dev/cu.usbserial-XXXXX1
✓ Router (6674): /dev/cu.usbserial-XXXXX2
✓ Gateway (D218): /dev/cu.usbserial-XXXXX3

Data collection started at 14:32:15
Progress: [=====>            ] 5/30 min (16%)
Progress: [===========>      ] 10/30 min (33%)
Progress: [================> ] 15/30 min (50%)
...
Progress: [====================] 30/30 min (100%)

Data collection complete!
Files saved:
  - experiments/results/flooding/run1_BB94_sensor.csv
  - experiments/results/flooding/run1_6674_router.csv
  - experiments/results/flooding/run1_D218_gateway.csv
```

---

### Step 6: Quick Data Validation

After the 30-minute run completes:

```bash
# Check files were created
ls -lh experiments/results/flooding/

# Should show 3 CSV files, each ~50-100 KB

# Quick peek at sensor data
head -20 experiments/results/flooding/run1_BB94_sensor.csv

# Should show CSV headers and data rows:
# timestamp,node_id,role,tx_count,rx_count,duty_cycle,memory_free_kb,...
```

**Sanity checks:**
- ✅ File size >10KB (if smaller, data might be incomplete)
- ✅ ~30 rows of monitoring stats (1 per minute)
- ✅ Duty-cycle stays <1% throughout
- ✅ No sudden memory drops

---

### Step 7: Run Remaining Flooding Tests

**Run 2:**
```bash
python3 utilities/data_collection.py \
  --protocol flooding \
  --run-number 2 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results
```

**Run 3:**
```bash
python3 utilities/data_collection.py \
  --protocol flooding \
  --run-number 3 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results
```

**Between runs:**
- Wait 2-3 minutes for nodes to stabilize
- Optionally reset nodes (RST button)
- Check no errors in previous run logs

**Flooding complete!** 🎉 You should now have:
```
experiments/results/flooding/
├── run1_BB94_sensor.csv
├── run1_6674_router.csv
├── run1_D218_gateway.csv
├── run2_BB94_sensor.csv
├── run2_6674_router.csv
├── run2_D218_gateway.csv
├── run3_BB94_sensor.csv
├── run3_6674_router.csv
└── run3_D218_gateway.csv
```

---

## 📱 PROTOCOL 2: HOP-COUNT ROUTING

### Step 8: Flash Hop-Count Firmware

**Disconnect all nodes from power for 10 seconds** (full reset).

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH/firmware/2_hopcount

# Flash sensor (BB94)
pio run -e sensor --target upload
# Verify: "Hop-Count Routing", "Role: SENSOR"

# Flash router (6674)  
pio run -e router --target upload
# Verify: "Hop-Count Routing", "Role: ROUTER"

# Flash gateway (D218)
pio run -e gateway --target upload
# Verify: "Hop-Count Routing", "Role: GATEWAY"
```

---

### Step 9: Verify Routing Table Building

Open 3 terminals again:

```bash
# Terminal 1: Sensor
screen /dev/cu.usbserial-XXXXX1 115200

# Terminal 2: Router
screen /dev/cu.usbserial-XXXXX2 115200

# Terminal 3: Gateway
screen /dev/cu.usbserial-XXXXX3 115200
```

**Wait 2-3 minutes** for HELLO packets to build routing tables.

**You should see** (every 30 seconds):
```
==== Routing Table ====
Routing table size: 2
Addr   Via    Hops  Role
------|------|------|----
6674 | 6674 |    1 | 00
D218 | 6674 |    2 | 04
=======================
```

**Key check:**
- Gateway (D218) appears in routing table
- Hop count = 2 (sensor → router → gateway)
- Sensor will now send to gateway address (not broadcast)

---

### Step 10: Run Hop-Count Tests

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

# Run 1
python3 utilities/data_collection.py \
  --protocol hopcount \
  --run-number 1 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results

# Run 2 (wait 2 min between runs)
python3 utilities/data_collection.py \
  --protocol hopcount \
  --run-number 2 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results

# Run 3
python3 utilities/data_collection.py \
  --protocol hopcount \
  --run-number 3 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results
```

**Hop-count complete!** 🎉

---

## 📱 PROTOCOL 3: COST ROUTING

### Step 11: Flash Cost Routing Firmware

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH/firmware/3_gateway_routing

# Flash sensor (BB94)
pio run -e sensor --target upload

# Flash router (6674)
pio run -e router --target upload

# Flash gateway (D218)
pio run -e gateway --target upload
```

---

### Step 12: Verify Gateway Discovery

Open terminals, check for:

```
==== Routing Table ====
Routing table size: 2
Addr   Via    Hops  Cost    GW Load
------|------|------|--------|--------
6674 | 6674 |    1 |   1.0  | 0.00%
D218 | 6674 |    2 |   2.0  | 0.50%
=======================

==== Gateway Load Stats ====
Gateway D218: Load=0.50%, Last update=12345ms ago
====================================
```

**Key difference from hop-count:**
- Cost metric includes gateway load
- Lower cost = better path
- Gateway load tracked in routing decisions

---

### Step 13: Run Cost Routing Tests

```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

# Run 1, 2, 3 (same as before)
python3 utilities/data_collection.py \
  --protocol cost_routing \
  --run-number [1/2/3] \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX1 \
  --router-port /dev/cu.usbserial-XXXXX2 \
  --gateway-port /dev/cu.usbserial-XXXXX3 \
  --output-dir experiments/results
```

**Cost routing complete!** 🎉

---

## ✅ Week 1 Testing Complete!

You should now have **27 CSV files**:

```bash
# Check all files exist
ls -R experiments/results/

experiments/results/flooding:
run1_BB94_sensor.csv    run2_BB94_sensor.csv    run3_BB94_sensor.csv
run1_6674_router.csv    run2_6674_router.csv    run3_6674_router.csv
run1_D218_gateway.csv   run2_D218_gateway.csv   run3_D218_gateway.csv

experiments/results/hopcount:
run1_BB94_sensor.csv    run2_BB94_sensor.csv    run3_BB94_sensor.csv
run1_6674_router.csv    run2_6674_router.csv    run3_6674_router.csv
run1_D218_gateway.csv   run2_D218_gateway.csv   run3_D218_gateway.csv

experiments/results/cost_routing:
run1_BB94_sensor.csv    run2_BB94_sensor.csv    run3_BB94_sensor.csv
run1_6674_router.csv    run2_6674_router.csv    run3_6674_router.csv
run1_D218_gateway.csv   run2_D218_gateway.csv   run3_D218_gateway.csv
```

---

## 🎯 Next: Week 2 Analytical Modeling

With hardware data collected, you can now:

```bash
# Run scalability analysis
python3 analysis/scalability_model.py \
  --data-dir experiments/results \
  --output-dir experiments/results/figures

# This will generate:
# - scalability_duty_cycle.png (main thesis figure!)
# - scalability_overhead.png
# - scalability_memory.png
# - breakpoint_analysis.csv
```

**The analytical model will:**
1. Load your hardware CSV files
2. Extract measured parameters (ToA, packet rate, node behavior)
3. Calculate duty-cycle for 10-100 nodes
4. Show flooding breaks at ~15 nodes, cost routing scales to ~50 nodes
5. Validate predictions match your 3-5 node measurements

---

## 📊 Quick Data Verification Script

```bash
# Check all files have data
for file in experiments/results/*/*.csv; do
  lines=$(wc -l < "$file")
  size=$(ls -lh "$file" | awk '{print $5}')
  echo "$file: $lines lines, $size"
done

# Should show ~20-40 lines per file, 20-100KB each
```

---

**Great job!** You're now ready for Week 2 analytical modeling! 🚀
