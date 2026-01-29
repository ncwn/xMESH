# xMESH 4-Hour Stability Test Procedure

## 1. Test Overview
- **Purpose**: Validate xMESH stability over 4 hours
- **Configuration**: 3 Heltec V3 nodes, indoor, all nodes visible
- **Duration**: 4 hours minimum

## 2. Hardware Setup
- **Node 1**: `/dev/cu.usbserial-0001` (gateway)
- **Node 2**: `/dev/cu.usbserial-4` (mesh node)
- **Node 3**: `/dev/cu.usbserial-5` (mesh node)
- **Placement**: All within 2 meters (indoor test)

## 3. Pre-Test Checklist
- [ ] All 3 nodes flashed with same firmware version
- [ ] Node 1 configured as gateway via `gateway on` command
- [ ] WiFi credentials set on gateway if testing OTA
- [ ] Serial monitors ready on all nodes

## 4. Monitoring Commands
```bash
# Start logging on each node (run in separate terminals)
pio device monitor -p /dev/cu.usbserial-0001 -b 115200 | tee node1.log
pio device monitor -p /dev/cu.usbserial-4 -b 115200 | tee node2.log
pio device monitor -p /dev/cu.usbserial-5 -b 115200 | tee node3.log
```

## 5. Metrics to Capture (every 60 minutes)
| Metric | How to Check | Target |
|--------|--------------|--------|
| Free heap | `status` command | > 50,000 bytes |
| Neighbor count | `status` command | 2 per node |
| Routing table size | `status` command | Stable (not growing) |
| Trickle TX count | `status` command | Increasing slowly |
| Trickle suppress count | `status` command | Should be > 0 |

## 6. Pass Criteria
All criteria must be met:
- [ ] **Heap stable**: Free heap > 50KB throughout test (no significant leak)
- [ ] **No crashes**: No watchdog resets (grep for "wdt", "rst:0x", "abort")
- [ ] **Routes stable**: Routing table size not growing unbounded
- [ ] **Mesh healthy**: All nodes see at least 1 neighbor at all times
- [ ] **No errors**: No ESP_LOGE messages indicating critical failures

## 7. Fail Criteria (stop test if seen)
- Heap drops below 20KB
- Repeated watchdog resets (> 3 in 1 hour)
- Node becomes unreachable

## 8. Post-Test Analysis
```bash
# Check for errors
grep -E "(ERROR|FAIL|abort|rst:0x|wdt)" node1.log node2.log node3.log

# Check heap readings
grep "heap" node1.log | head -10
grep "heap" node1.log | tail -10

# Save summary
cat > stability-test.log << EOF
xMESH 4-Hour Stability Test
Date: \$(date)
Duration: 4 hours
Result: [PASS/FAIL]

Heap (start): XXX bytes
Heap (end): XXX bytes
Heap change: XXX bytes

Neighbor count stable: [YES/NO]
Route count stable: [YES/NO]
WDT resets: [count]
Error count: [count]
EOF
```

## 9. Evidence to Capture
Save these files to `.sisyphus/evidence/`:
- `stability-test.log` - Summary with PASS/FAIL verdict
- `node1-4hr.log` - Full log from gateway node (optional, can be large)
