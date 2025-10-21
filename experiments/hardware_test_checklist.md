# Hardware Testing Checklist - Week 1

**Date Range**: October 21-24, 2025  
**Objective**: Collect empirical consumption data from all 3 protocols  
**Goal**: 9 successful test runs (3 protocols × 3 runs × 30 min each)

---

## 📦 Equipment Checklist

### Hardware (Before Each Test Day)
- [ ] **Node BB94** (Sensor) - Fully charged, firmware flashed
- [ ] **Node 6674** (Router) - Fully charged, firmware flashed  
- [ ] **Node D218** (Gateway) - Fully charged, firmware flashed
- [ ] **3× USB-C cables** - For serial monitoring
- [ ] **MacBook** - For data collection
- [ ] **Power bank (optional)** - For extended tests

### Physical Setup
- [ ] **Test location** - Clear line of sight, minimal interference
- [ ] **Distance markers** - 5m between nodes (BB94 ↔ 6674 ↔ D218)
- [ ] **Test log notebook** - Record observations, issues, environmental notes

### Software Preparation
- [ ] **PlatformIO** - Installed and working (`pio --version`)
- [ ] **data_collection.py** - Tested and working
- [ ] **3× Terminal windows** - For monitoring all nodes simultaneously
- [ ] **Screen/tmux (optional)** - For managing multiple terminals

---

## 🏗️ Node Configuration

### Node Roles (Do NOT change these!)
Based on compile flags in `common/heltec_v3_config.h`:

| Node ID | MAC Address | Role | Behavior |
|---------|-------------|------|----------|
| **BB94** | `0xBB94` | **SENSOR** | Sends data every 60s, rebroadcasts (flooding) or forwards (routing) |
| **6674** | `0x6674` | **ROUTER** | Forwards packets, no data generation |
| **D218** | `0xD218` | **GATEWAY** | Receives packets, logs arrivals, no rebroadcast |

### Physical Topology
```
SENSOR (BB94) <--5m--> ROUTER (6674) <--5m--> GATEWAY (D218)
```

This linear topology ensures:
- Multi-hop routing (sensor can't reach gateway directly)
- Router must forward packets
- Clear path for analyzing hop behavior

---

## 📊 Test Matrix

### Protocol 1: Flooding (O(N²) Baseline)
**Firmware Version**: v0.2.0-alpha  
**Expected Behavior**: All nodes rebroadcast everything (high overhead)

| Run | Date | Start Time | Duration | Status | Notes |
|-----|------|------------|----------|--------|-------|
| 1 | Oct 21 | _________ | 30 min | [ ] | _________________ |
| 2 | Oct 21 | _________ | 30 min | [ ] | _________________ |
| 3 | Oct 21 | _________ | 30 min | [ ] | _________________ |

**Expected Monitoring Output**:
- Duty-cycle: ~0.2-0.5% (frequent rebroadcasts)
- Memory: Stable (~21KB RAM)
- Queue: Low drops (simple logic)

---

### Protocol 2: Hop-Count Routing (O(N√N) Baseline)
**Firmware Version**: v0.3.1-alpha  
**Expected Behavior**: Route via shortest path (hop count metric)

| Run | Date | Start Time | Duration | Status | Notes |
|-----|------|------------|----------|--------|-------|
| 1 | Oct 22 | _________ | 30 min | [ ] | _________________ |
| 2 | Oct 22 | _________ | 30 min | [ ] | _________________ |
| 3 | Oct 22 | _________ | 30 min | [ ] | _________________ |

**Expected Monitoring Output**:
- Duty-cycle: ~0.1-0.3% (selective forwarding)
- Memory: Stable (~21KB RAM + routing table)
- Routing table: Should show 2-hop path to gateway
- Queue: Very low drops

---

### Protocol 3: Cost Routing (O(0.8N√N) Optimized)
**Firmware Version**: v0.4.0-alpha  
**Expected Behavior**: Gateway-aware cost routing (lowest overhead)

| Run | Date | Start Time | Duration | Status | Notes |
|-----|------|------------|----------|--------|-------|
| 1 | Oct 23 | _________ | 30 min | [ ] | _________________ |
| 2 | Oct 23 | _________ | 30 min | [ ] | _________________ |
| 3 | Oct 23 | _________ | 30 min | [ ] | _________________ |

**Expected Monitoring Output**:
- Duty-cycle: ~0.05-0.2% (optimized paths + gateway load balancing)
- Memory: Stable (~21KB RAM + routing table)
- Gateway load stats: Should show load-aware routing
- Queue: Minimal drops

---

## 📝 Data Collection Files

### Directory Structure (Auto-created by script)
```
experiments/results/
├── flooding/
│   ├── run1_BB94_sensor.csv
│   ├── run1_6674_router.csv
│   ├── run1_D218_gateway.csv
│   ├── run2_BB94_sensor.csv
│   ├── run2_6674_router.csv
│   ├── run2_D218_gateway.csv
│   ├── run3_BB94_sensor.csv
│   ├── run3_6674_router.csv
│   └── run3_D218_gateway.csv
├── hopcount/
│   ├── run1_BB94_sensor.csv
│   ├── run1_6674_router.csv
│   ├── run1_D218_gateway.csv
│   ├── (... run2, run3 ...)
└── cost_routing/
    ├── run1_BB94_sensor.csv
    ├── run1_6674_router.csv
    ├── run1_D218_gateway.csv
    └── (... run2, run3 ...)
```

**Total**: 27 CSV files (9 runs × 3 nodes)

### CSV Content (Captured by data_collection.py)
Each file contains:
- Timestamp
- TX/RX packet counts
- Duty-cycle percentage
- Memory usage (current/min/max)
- Queue statistics (enqueued/dropped/depth)
- Protocol-specific info (routing table size, gateway load, etc.)

---

## ⚠️ Common Issues & Troubleshooting

### Issue 1: Node Won't Flash
**Symptoms**: Upload fails, "failed to connect" error  
**Solutions**:
- Hold BOOT button while connecting USB
- Try different USB-C cable
- Check `pio device list` shows the port
- Reset board (RST button)

### Issue 2: No Serial Output
**Symptoms**: Terminal blank after flashing  
**Solutions**:
- Check baud rate: `115200`
- Try different terminal: `screen /dev/cu.usbserial-* 115200`
- Reset board after flashing
- Check USB cable (some are charge-only)

### Issue 3: Nodes Can't Communicate
**Symptoms**: No RX packets, routing table empty  
**Solutions**:
- Check distance (try 2-3m first, then increase)
- Verify all nodes on same LoRa parameters (SF7, BW125, CR4/5)
- Check antenna connections
- Reduce distance to test connectivity

### Issue 4: High Packet Loss
**Symptoms**: <80% PDR, many dropped packets  
**Solutions**:
- Move nodes closer together
- Check for WiFi/Bluetooth interference (disable on MacBook)
- Ensure clear line of sight
- Test each link individually (sensor↔router, router↔gateway)

### Issue 5: Data Collection Script Crashes
**Symptoms**: Python error, script exits early  
**Solutions**:
- Check Python version: `python3 --version` (3.8+)
- Install dependencies: `pip3 install pyserial pandas`
- Check serial permissions: `ls -l /dev/cu.usbserial-*`
- Verify port names match

---

## 🎯 Success Criteria (Per Test Run)

### Must Have:
- ✅ **Duration**: Full 30 minutes captured
- ✅ **All 3 nodes**: Data from sensor, router, gateway
- ✅ **Monitoring stats**: Duty-cycle, memory, queue data present
- ✅ **Packet flow**: Sensor sends, gateway receives (>80% PDR)

### Good to Have:
- ✅ **Stable duty-cycle**: <1% throughout test
- ✅ **No crashes**: All nodes stay online
- ✅ **Clean logs**: No error spam in serial output
- ✅ **Consistent behavior**: Similar results across 3 runs

### Red Flags (Investigate):
- ❌ Duty-cycle >1% (regulatory violation)
- ❌ PDR <50% (link quality issues)
- ❌ Memory drops >20% (memory leak)
- ❌ Queue drops >10% (buffer saturation)
- ❌ Node crashes/reboots mid-test

---

## 📈 Expected Timeline

### Day 1 (Oct 21 - Today!): Flooding Protocol
- **Morning**: Hardware prep, firmware flash, test setup validation
- **Afternoon**: Run flooding tests (3 × 30 min)
- **Evening**: Quick data validation, backup files

### Day 2 (Oct 22): Hop-Count Protocol  
- **Morning**: Flash hop-count firmware, verify routing table
- **Afternoon**: Run hop-count tests (3 × 30 min)
- **Evening**: Data validation, compare with flooding

### Day 3 (Oct 23): Cost Routing Protocol
- **Morning**: Flash cost routing firmware, verify gateway discovery
- **Afternoon**: Run cost routing tests (3 × 30 min)
- **Evening**: Data validation, final backup

### Day 4 (Oct 24): Buffer Day
- **Use for**: Re-runs if any test failed, data verification, documentation
- **If all good**: Start Week 2 analytical modeling early!

---

## 💾 Data Backup Strategy

After EACH test session:
```bash
# Backup to separate directory
cp -r experiments/results experiments/results_backup_$(date +%Y%m%d_%H%M%S)

# Optional: Commit to git
git add experiments/results
git commit -m "data: Add [protocol] run [N] - [date]"
```

**Never delete original data** until thesis submitted!

---

## 📞 Emergency Contacts

If major issues arise:
- **Advisor**: [Your advisor's contact]
- **LoRaMesher GitHub**: https://github.com/LoRaMesher/LoRaMesher/issues
- **Heltec Forum**: https://heltec.org/community

---

**Next**: Proceed to Step-by-Step Testing Instructions →
