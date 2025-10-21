# 📋 COMPLETE TESTING GUIDE SUMMARY

**Date**: October 21, 2025  
**Your Mission**: Collect hardware data from 3 protocols for analytical scalability modeling  
**Timeline**: 2-3 days (Oct 21-23), buffer on Oct 24

---

## 🎯 What You Need to Do

### The Simple Version:
1. **Flash firmware** to 3 nodes (BB94, 6674, D218)
2. **Run data collection script** for 30 minutes (repeat 3 times)
3. **Repeat for 3 protocols** (flooding, hop-count, cost routing)
4. **Result**: 27 CSV files ready for Week 2 analysis

### The Commands You'll Use Most:

**Flash Firmware:**
```bash
cd firmware/[1_flooding | 2_hopcount | 3_gateway_routing]
pio run -e sensor --target upload    # Connect BB94
pio run -e router --target upload    # Connect 6674
pio run -e gateway --target upload   # Connect D218
```

**Run Data Collection:**
```bash
python3 utilities/multi_node_data_collection.py \
  --protocol [flooding|hopcount|cost_routing] \
  --run-number [1|2|3] \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX \
  --router-port /dev/cu.usbserial-YYYYY \
  --gateway-port /dev/cu.usbserial-ZZZZZ \
  --output-dir experiments/results
```

---

## 📚 Documentation Created for You

### 1. **QUICKSTART_TESTING.md** ⭐ START HERE!
**Location**: `experiments/QUICKSTART_TESTING.md`

**What's in it:**
- ⚡ Super quick TL;DR commands
- 📖 Detailed first-time walkthrough
- 🐛 Troubleshooting common issues
- ✅ Success checklist
- ⏱️ Time estimates

**When to use**: First read, quick reference during testing

---

### 2. **testing_instructions.md** - Detailed Step-by-Step
**Location**: `experiments/testing_instructions.md`

**What's in it:**
- Step 0: Verify environment ready
- **Step 1-7: Flooding protocol** (flash, test, validate)
- **Step 8-10: Hop-count protocol** (flash, test, validate)
- **Step 11-13: Cost routing protocol** (flash, test, validate)
- Quick data verification commands

**When to use**: During actual testing, follow along step-by-step

---

### 3. **hardware_test_checklist.md** - Planning & Tracking
**Location**: `experiments/hardware_test_checklist.md`

**What's in it:**
- 📦 Equipment checklist (hardware, software, cables)
- 🏗️ Node configuration reference (BB94=sensor, 6674=router, D218=gateway)
- 📊 Test matrix with tracking tables (fill in as you complete tests)
- ⚠️ Common issues & troubleshooting guide
- 📈 Expected timeline (Day 1: flooding, Day 2: hop-count, Day 3: cost routing)
- 💾 Data backup strategy
- 🎯 Success criteria per test run

**When to use**: Before starting, during test tracking, after completion validation

---

### 4. **multi_node_data_collection.py** - The Magic Script ✨
**Location**: `utilities/multi_node_data_collection.py`

**What it does:**
- Connects to ALL 3 nodes simultaneously via USB
- Captures serial output from sensor, router, gateway
- Parses monitoring stats:
  - Duty-cycle percentage
  - Memory usage (current/min/max)
  - Queue statistics (enqueued/dropped)
  - TX/RX packet counts
- Saves to 3 separate CSV files (one per node)
- Shows real-time progress bar
- Prints summary statistics when complete

**Why it's awesome:**
- No manual copy-paste from terminals!
- Synchronized data collection across all nodes
- Automatic parsing and CSV formatting
- Progress updates so you know it's working

---

## 🔑 Key Information

### Node Configuration (FIXED - Don't Change!)
| Node ID | MAC | Role | Behavior |
|---------|-----|------|----------|
| **BB94** | 0xBB94 | SENSOR | Sends data every 60s |
| **6674** | 0x6674 | ROUTER | Forwards packets |
| **D218** | 0xD218 | GATEWAY | Receives and logs |

### Physical Setup
```
[BB94 Sensor] ←----- 5m -----→ [6674 Router] ←----- 5m -----→ [D218 Gateway]
```

### Firmware Versions
| Protocol | Version | Folder | Behavior |
|----------|---------|--------|----------|
| **Flooding** | v0.2.0-alpha | `firmware/1_flooding` | All nodes rebroadcast (O(N²)) |
| **Hop-count** | v0.3.1-alpha | `firmware/2_hopcount` | Shortest path routing (O(N√N)) |
| **Cost routing** | v0.4.0-alpha | `firmware/3_gateway_routing` | Gateway-aware cost (O(0.8N√N)) |

### Test Matrix (Track Your Progress)
| Protocol | Run 1 | Run 2 | Run 3 | Status |
|----------|-------|-------|-------|--------|
| **Flooding** | [ ] | [ ] | [ ] | ⬜ Pending |
| **Hop-count** | [ ] | [ ] | [ ] | ⬜ Pending |
| **Cost routing** | [ ] | [ ] | [ ] | ⬜ Pending |

**Goal**: All 9 runs complete = 27 CSV files ✅

---

## 🚀 Starting Today (Oct 21)

### Before You Begin:
1. ✅ Read `experiments/QUICKSTART_TESTING.md` (5 min)
2. ✅ Check hardware ready:
   - [ ] 3 Heltec nodes charged
   - [ ] 3 USB-C cables
   - [ ] MacBook ready
3. ✅ Check software ready:
   ```bash
   pio --version  # Should show 6.x.x
   python3 --version  # Should show 3.8+
   ```

### Your First Test (Flooding Run 1):

**Step 1: Flash firmware** (5 min)
```bash
cd /Volumes/Data/xMESH/xLAB/xMESH/firmware/1_flooding

# Connect BB94 via USB
pio run -e sensor --target upload
# Wait for "SUCCESS"

# Connect 6674 via USB
pio run -e router --target upload

# Connect D218 via USB
pio run -e gateway --target upload
```

**Step 2: Find ports** (2 min)
```bash
pio device list
# Shows 3 ports: /dev/cu.usbserial-XXXXX

# Test each port to identify which is which:
screen /dev/cu.usbserial-0001 115200
# Look for "Local address: BB94" (or 6674, D218)
# Press Ctrl+A then K to exit
```

**Step 3: Run test** (30 min)
```bash
cd /Volumes/Data/xMESH/xLAB/xMESH

python3 utilities/multi_node_data_collection.py \
  --protocol flooding \
  --run-number 1 \
  --duration 30 \
  --sensor-port /dev/cu.usbserial-XXXXX \
  --router-port /dev/cu.usbserial-YYYYY \
  --gateway-port /dev/cu.usbserial-ZZZZZ \
  --output-dir experiments/results
```

**Step 4: Verify** (1 min)
```bash
# Check files created
ls -lh experiments/results/flooding/

# Should see 3 files, each 20-100 KB:
# run1_BB94_sensor.csv
# run1_6674_router.csv
# run1_D218_gateway.csv
```

**Step 5: Repeat** (2x more)
Wait 2 minutes, then repeat Step 3 with `--run-number 2`, then `--run-number 3`.

**Flooding complete!** ✅

---

## ⏱️ Time Budget

| Day | Protocol | Runs | Time | Activities |
|-----|----------|------|------|------------|
| **Oct 21** | Flooding | 3 | ~2 hrs | Flash, test 3×30min, verify |
| **Oct 22** | Hop-count | 3 | ~2 hrs | Flash, test 3×30min, verify |
| **Oct 23** | Cost routing | 3 | ~2 hrs | Flash, test 3×30min, verify |
| **Oct 24** | Buffer | - | - | Re-runs if needed, or start Week 2! |

**Total**: ~6 hours spread over 3 days (very doable!)

---

## 🐛 Quick Troubleshooting

### "Port not found"
```bash
ls /dev/cu.*  # Use actual port names shown
```

### "Permission denied"
```bash
chmod +x utilities/multi_node_data_collection.py
pip3 install pyserial
```

### "Nodes don't communicate"
1. Reduce distance to 2-3 meters
2. Check antennas connected and vertical
3. Reset all nodes (RST button)
4. Disable WiFi on MacBook (interference)

### "Script shows no monitoring updates"
1. Verify firmware has monitoring code (should - you compiled it!)
2. Check serial output manually: `screen /dev/cu.usbserial-XXXXX 115200`
3. Look for "==== Network Monitoring Stats ====" every 30 seconds
4. If missing, reflash firmware

---

## 📊 What Success Looks Like

After each test run, you should see:

**Console Output:**
```
=== LoRa Mesh Network Data Collection ===
Protocol: flooding
Run: 1
Duration: 30 minutes

Connecting to nodes...
✓ SENSOR (BB94): /dev/cu.usbserial-0001
✓ ROUTER (6674): /dev/cu.usbserial-0002
✓ GATEWAY (D218): /dev/cu.usbserial-0003

Data collection started at 14:30:00
Progress: [====================] 30/30 min (100%)

Data collection complete!

=== Collection Summary ===
SENSOR (BB94):
  Monitoring updates: 60
  Max duty-cycle: 0.150%
  Min free memory: 285 KB
  Queue drops: 0
  TX packets: 30
  RX packets: 5

ROUTER (6674):
  Monitoring updates: 60
  Max duty-cycle: 0.200%
  Min free memory: 283 KB
  Queue drops: 0
  TX packets: 60
  RX packets: 30

GATEWAY (D218):
  Monitoring updates: 60
  Max duty-cycle: 0.000%
  Min free memory: 290 KB
  Queue drops: 0
  TX packets: 0
  RX packets: 60

Files saved:
  - experiments/results/flooding/run1_BB94_sensor.csv
  - experiments/results/flooding/run1_6674_router.csv
  - experiments/results/flooding/run1_D218_gateway.csv
```

**CSV Files:**
```csv
timestamp,node_id,role,duty_cycle_pct,tx_count,violations,memory_free_kb,...
2025-10-21T14:30:15,BB94,SENSOR,0.125,1,0,285,320,280,35,...
2025-10-21T14:30:45,BB94,SENSOR,0.150,1,0,283,320,280,37,...
2025-10-21T14:31:15,BB94,SENSOR,0.145,1,0,284,320,280,36,...
...
```

✅ **Good signs:**
- All 3 nodes connected
- 60 monitoring updates (1 every 30 seconds × 30 minutes = 60)
- Duty-cycle <1% (regulatory compliant!)
- Memory stable (no big drops)
- No queue drops (or very few)
- CSV files 20-100 KB each

❌ **Red flags:**
- <30 monitoring updates (data collection interrupted)
- Duty-cycle >1% (at 3 nodes, should be ~0.1-0.5%)
- Memory drops >50 KB (possible memory leak)
- Queue drops >10% (saturation issue)
- CSV files <10 KB (incomplete data)

---

## 🎯 After Week 1 Complete

You'll have:
```
experiments/results/
├── flooding/ (9 CSV files)
├── hopcount/ (9 CSV files)
└── cost_routing/ (9 CSV files)
```

**Then Week 2 starts!**

```bash
# Run analytical scalability model
python3 analysis/scalability_model.py \
  --data-dir experiments/results \
  --output-dir experiments/results/figures

# Generates:
# - scalability_duty_cycle.png ← MAIN THESIS FIGURE!
# - scalability_overhead.png
# - scalability_memory.png
# - breakpoint_analysis.csv
```

This shows:
- **Flooding breaks at ~15 nodes** (duty-cycle >1%)
- **Hop-count breaks at ~40 nodes**
- **Cost routing scales to ~50 nodes** ✨
- Your hardware data validates the model at 3 nodes ✅

---

## 📞 Need Help?

**Before asking:**
1. ✅ Check `experiments/hardware_test_checklist.md` troubleshooting section
2. ✅ Re-read relevant section in `experiments/testing_instructions.md`
3. ✅ Try reducing node distance to 2-3m
4. ✅ Try reflashing firmware

**When to contact advisor:**
- Can't get nodes to communicate after 1 hour troubleshooting
- Hardware appears faulty (crashes, won't flash)
- Data looks completely wrong across multiple runs
- Schedule impact (can't complete Week 1 by Oct 24)

---

## 🎉 You're Ready!

Everything you need:
- ✅ Firmware with monitoring compiled (Week 1 prep complete!)
- ✅ Data collection script ready (`multi_node_data_collection.py`)
- ✅ Testing instructions written (3 guides to choose from)
- ✅ Analytical model ready for Week 2 (`scalability_model.py`)
- ✅ Clear timeline (Oct 21-24 testing, Oct 25+ analysis)

**Start with**: `experiments/QUICKSTART_TESTING.md`

**Good luck! You got this! 🚀**

---

**Next update**: After completing flooding tests, update this document with actual results and any lessons learned!
