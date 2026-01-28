# xMESH Stability Test Plan

**Document Version**: 1.0  
**Date Created**: 2026-01-29  
**Target Firmware**: xMESH Production v1.0

## Overview

This document defines the stability and reliability testing requirements for the xMESH production firmware. The test validates error handling, watchdog protection, heap management, and long-term operational stability.

## Test Environment

### Hardware Requirements
- **Minimum 3 nodes**: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
- **1 Gateway node** with WiFi connectivity
- **2+ Mesh nodes** positioned for multi-hop routing
- **Power**: USB or battery (stable power source required)

### Software Requirements
- xMESH Production Firmware (with error handling and watchdog)
- PlatformIO for firmware upload
- Serial monitoring tool (115200 baud)

## Test Duration

### Minimum Test Duration
- **Short-term stability**: 4 hours continuous operation
- **Long-term stability**: 24-48 hours continuous operation

### Test Phases
1. **Initialization Phase** (0-5 minutes): Verify all nodes boot successfully
2. **Mesh Formation Phase** (5-30 minutes): Verify routing tables converge
3. **Steady State Phase** (30 minutes - test end): Monitor for stability issues
4. **Recovery Phase** (periodic): Test node failure and recovery

## Pass Criteria

### Critical Requirements (Must Pass)
1. **Zero Watchdog Resets**: No ESP32 watchdog timeouts during test period
2. **Heap Stability**: Free heap remains above 15KB (15360 bytes) threshold
3. **No ESP_LOGE in Steady State**: Zero error-level logs after mesh stabilization
4. **Routing Convergence**: All nodes establish routes within 5 minutes
5. **No Crashes**: No unexpected reboots or panics

### Performance Requirements
1. **Trickle Suppression**: Suppression ratio > 30% in stable network
2. **ETX Tracking**: All active links show ETX values between 1.0-3.0
3. **Gateway Health**: Neighbor health monitoring detects failures within 6 minutes
4. **Memory Leaks**: Heap usage variation < 5% over 24 hours

## Test Metrics to Collect

### System Health Metrics
- **Heap Usage**: Record `esp_get_free_heap_size()` every 60 seconds
- **Watchdog Events**: Count watchdog resets (should be 0)
- **Uptime**: Total runtime without reboot
- **ESP_LOGE Count**: Number of error-level log events

### Network Metrics
- **Routing Table Size**: Number of active routes per node
- **Hello Transmission Rate**: Actual vs. expected (Trickle interval)
- **Hello Suppression Count**: From `trickle.getSuppressCount()`
- **Packet Loss**: Derived from ETX values

### Link Quality Metrics
- **RSSI Range**: Expected -120 to -30 dBm
- **SNR Range**: Expected -10 to +10 dB
- **ETX Range**: Expected 1.0 to 3.0 (values > 5.0 indicate poor links)

## Test Procedure

### 1. Pre-Test Setup
```bash
# Flash firmware to all nodes
pio run -t upload

# Connect serial monitors to all nodes
pio device monitor
```

### 2. Test Execution
1. Power on all nodes simultaneously (±10 seconds)
2. Record timestamp: `TEST_START = <timestamp>`
3. Monitor serial output from all nodes
4. Every 60 seconds, record:
   - Free heap size from `[HEALTH]` logs
   - Current Trickle interval from `[Trickle]` logs
   - Failed neighbor count from `[GatewayBalancer]` logs

### 3. Induced Failure Tests (Optional)
- **Node Failure**: Power off a mesh node and verify:
  - Neighbors detect failure within 6 minutes
  - Routing table updates within 10 minutes
  - Traffic reroutes automatically
- **Node Recovery**: Power on the failed node and verify:
  - Node rejoins mesh within 5 minutes
  - `[HEALTH] RECOVERED` message appears

### 4. Test Completion
1. Record timestamp: `TEST_END = <timestamp>`
2. Calculate total uptime for each node
3. Extract final metrics from serial logs
4. Power down all nodes

## Log Analysis

### Key Log Patterns to Search

**Error Handling Verification**
```bash
# Count ESP_LOGE events (should be minimal after 30 min)
grep "E (" serial_log.txt | wc -l

# Count ESP_LOGW events (should be reasonable)
grep "W (" serial_log.txt | wc -l

# Check for watchdog resets (should be zero)
grep "Task watchdog" serial_log.txt
```

**Heap Monitoring**
```bash
# Extract heap samples (should stay above 15KB)
grep "Free heap:" serial_log.txt

# Check for low memory warnings
grep "Low heap memory" serial_log.txt
```

**Routing Stability**
```bash
# Verify Trickle adaptation
grep "DOUBLE - I=" serial_log.txt

# Check suppression events
grep "SUPPRESS" serial_log.txt
```

## Expected Error/Warning Logs (Normal Operation)

### Acceptable Warnings (ESP_LOGW)
- `"Suspicious RSSI value"`: May occur at network edge (distance-dependent)
- `"Link map full"`: Expected when tracking > 10 neighbors (FIFO eviction)
- `"Trickle disabled"`: Only if explicitly disabled in config
- `"Low heap memory"`: Should NOT occur (indicates problem)

### Unacceptable Errors (ESP_LOGE)
- `"Invalid weight configuration"`: Indicates config.h corruption
- `"Invalid interval config"`: Indicates Trickle misconfiguration
- `"Cannot add neighbor"`: Indicates neighbor table overflow (max 20)
- Watchdog timeout messages: System instability

## Test Report Template

```markdown
# xMESH Stability Test Report

**Test Date**: YYYY-MM-DD  
**Test Duration**: X hours  
**Tester**: [Name]

## Test Configuration
- Number of nodes: X
- Topology: [Linear / Star / Mesh]
- Power source: [USB / Battery]

## Results Summary
- [ ] Zero watchdog resets: PASS / FAIL
- [ ] Heap > 15KB throughout: PASS / FAIL
- [ ] No ESP_LOGE in steady state: PASS / FAIL
- [ ] Routing convergence < 5 min: PASS / FAIL
- [ ] No unexpected reboots: PASS / FAIL

## Metrics
- Minimum free heap: XXXXX bytes
- ESP_LOGE count: X
- ESP_LOGW count: X
- Suppression ratio: XX%
- Average ETX: X.XX

## Issues Encountered
[List any anomalies, crashes, or unexpected behavior]

## Conclusion
PASS / FAIL

## Recommendations
[Any firmware improvements or config changes needed]
```

## Automation (Future Work)

### Suggested Automation Scripts
1. **Log Parser**: Python script to extract metrics from serial logs
2. **Heap Tracker**: Real-time heap usage plotting
3. **Alert System**: Trigger notifications on ESP_LOGE or low heap
4. **ETX Analyzer**: Calculate delivery ratios and link quality statistics

### Example Heap Monitoring Script
```python
import re
import matplotlib.pyplot as plt

heap_values = []
timestamps = []

with open('serial_log.txt', 'r') as f:
    for line in f:
        match = re.search(r'\[HEALTH\] Free heap: (\d+) bytes', line)
        if match:
            heap_values.append(int(match.group(1)))

# Plot heap usage over time
plt.plot(heap_values)
plt.axhline(y=15360, color='r', linestyle='--', label='15KB Threshold')
plt.xlabel('Sample Index')
plt.ylabel('Free Heap (bytes)')
plt.title('xMESH Heap Usage Over Time')
plt.legend()
plt.savefig('heap_usage.png')
```

## References

- [ESP32 Task Watchdog Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html)
- [ESP32 Heap Memory Debugging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html)
- RFC 6206: Trickle Algorithm Specification

---

**Document Control**  
Last Updated: 2026-01-29  
Next Review: After first stability test completion
