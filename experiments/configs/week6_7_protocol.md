# Week 6-7: Performance Evaluation Protocol

## Objective
Quantitatively compare three routing protocols:
1. **Flooding Baseline** (v0.2.0-alpha)
2. **Hop-Count Routing** (v0.3.1-alpha)
3. **Gateway-Aware Cost Routing** (v0.4.0-alpha)

---

## Test Matrix

### Protocols Under Test
| Protocol | Version | Key Feature | Expected Behavior |
|----------|---------|-------------|-------------------|
| Flooding | v0.2.0-alpha | Broadcast with duplicate detection | High overhead, simple reliability |
| Hop-Count | v0.3.1-alpha | Minimum hop distance vector | Efficient routing, hop-based optimization |
| Cost Routing | v0.4.0-alpha | Multi-factor cost (RSSI, SNR, ETX) | Link-quality aware, adaptive routing |

### Test Scenarios

#### Scenario 1: Linear Topology (Baseline)
```
[Sensor] -------- [Router] -------- [Gateway]
   BB94             6674               D218
Distance: ~5m between nodes
```
- **Purpose:** Establish baseline performance in controlled environment
- **Variables:** Protocol only (topology fixed)
- **Duration:** 30 minutes per protocol
- **Repetitions:** 3 runs per protocol (statistical validity)

#### Scenario 2: Distance Variation
```
[Sensor] ---- [Router] ---- [Gateway]
     5m            5m            (Fixed)
     
[Sensor] ---------- [Gateway]
         10m           (Direct, no router)
         
[Sensor] ------- [Router] ------- [Gateway]
    10m              10m
```
- **Purpose:** Measure impact of distance on PDR and latency
- **Variables:** Distance (5m, 10m, 15m)
- **Duration:** 20 minutes per distance × protocol
- **Repetitions:** 2 runs per configuration

#### Scenario 3: Network Load (if time permits)
```
Multiple sensors transmitting simultaneously
Rate: 1 pkt/30s vs 1 pkt/10s
```
- **Purpose:** Evaluate scalability and collision handling
- **Variables:** Packet rate, number of active sensors
- **Duration:** 20 minutes per load level
- **Repetitions:** 2 runs per configuration

---

## Metrics to Collect

### Primary Metrics

#### 1. Packet Delivery Ratio (PDR)
- **Definition:** `PDR = (Packets Received at Gateway) / (Packets Sent by Sensor)`
- **Target:** PDR ≥ 95% for functional network
- **Measurement:** 
  - Count sensor TX from serial log (Seq numbers)
  - Count gateway RX from serial log
  - Calculate ratio per test run
- **Expected Results:**
  - Flooding: High PDR (redundant paths)
  - Hop-Count: High PDR (reliable routing)
  - Cost Routing: Similar to hop-count, possibly better in poor conditions

#### 2. End-to-End Latency
- **Definition:** Time from sensor TX to gateway RX
- **Target:** < 2 seconds for responsive network
- **Measurement:**
  - Synchronized timestamps (NTP or manual sync)
  - Sensor logs: "TX timestamp"
  - Gateway logs: "RX timestamp"
  - Calculate delta per packet
- **Statistical Analysis:**
  - Mean, median, 95th percentile
  - CDF plots for distribution visualization
- **Expected Results:**
  - Flooding: Higher latency (multiple transmissions)
  - Hop-Count: Lower latency (direct routing)
  - Cost Routing: Similar to hop-count

#### 3. Network Overhead
- **Definition:** Total packets transmitted vs useful data delivered
- **Components:**
  - Control packets (HELLO, route updates)
  - Data packet retransmissions
  - Routing header overhead
- **Measurement:**
  - Count all TX across all nodes
  - Ratio: `Overhead = Total TX / Data Packets Delivered`
- **Expected Results:**
  - Flooding: Very high (every packet broadcast by all nodes)
  - Hop-Count: Moderate (HELLO packets + efficient routing)
  - Cost Routing: Similar to hop-count (no additional control traffic)

### Secondary Metrics

#### 4. Route Stability
- **Definition:** Frequency of route changes over time
- **Measurement:**
  - Log routing table changes ("Route to X changed from Y to Z")
  - Count route flaps per hour
- **Expected Results:**
  - Flooding: N/A (no routing)
  - Hop-Count: Stable (only changes on topology change)
  - Cost Routing: Potentially more dynamic (responds to link quality)

#### 5. Link Quality Metrics (Cost Routing Only)
- **RSSI Range:** Track min/max/avg RSSI per link
- **SNR Range:** Track min/max/avg SNR per link
- **Cost Evolution:** How does cost change over time?
- **Purpose:** Validate cost function behavior

---

## Test Procedure

### Pre-Test Setup
1. **Hardware Preparation:**
   - Charge all 3 Heltec V3 nodes fully
   - Label nodes clearly (Sensor BB94, Router 6674, Gateway D218)
   - Prepare USB cables and power adapters

2. **Environment Setup:**
   - Measure and mark distances (5m, 10m, 15m markers)
   - Choose location with minimal 915 MHz interference
   - Document environmental conditions (indoor/outdoor, obstacles)

3. **Software Preparation:**
   - Clone or checkout appropriate firmware version
   - Verify compilation for all 3 roles
   - Prepare data collection scripts

### Test Execution (Per Protocol)

#### Phase 1: Flash Firmware
```bash
# Example for flooding baseline
cd firmware/1_flooding
pio run -e sensor -t upload    # Flash BB94
pio run -e router -t upload    # Flash 6674
pio run -e gateway -t upload   # Flash D218
```

#### Phase 2: Deploy Nodes
- Place nodes at measured distances
- Connect gateway to laptop via USB for logging
- Power sensor and router independently (battery or USB power bank)

#### Phase 3: Start Monitoring
```bash
# Gateway serial monitor with timestamp logging
python utilities/data_collection.py --port /dev/cu.usbserial-0001 --output experiments/results/flooding/run1.csv
```

#### Phase 4: Run Test
- Let system run for specified duration (20-30 minutes)
- Monitor for anomalies (crashes, disconnections)
- Record observations in test log

#### Phase 5: Data Collection
- Stop monitoring script
- Download sensor/router logs if accessible
- Save CSV files with timestamp
- Back up raw serial logs

#### Phase 6: Repeat
- Perform 2-3 repetitions per configuration
- Power cycle nodes between runs
- Randomize test order to avoid bias

---

## Data Collection Format

### CSV Output Format
```csv
timestamp,node_id,event_type,seq_num,src_addr,dst_addr,hops,value,rssi,snr,latency_ms
2025-10-18 14:23:45.123,D218,RX,1,BB94,D218,0,62.29,-108,-10,1234
2025-10-18 14:24:15.456,D218,RX,2,BB94,D218,0,0.08,-108,-10,1456
...
```

### Required Data Fields
- **timestamp:** ISO 8601 format with milliseconds
- **node_id:** 4-character hex address
- **event_type:** TX, RX, ROUTE_CHANGE, HELLO
- **seq_num:** Packet sequence number
- **src_addr:** Source node address
- **dst_addr:** Destination node address
- **hops:** Hop count (from packet)
- **value:** Sensor data payload (if applicable)
- **rssi:** RSSI in dBm (if available)
- **snr:** SNR in dB (if available)
- **latency_ms:** Calculated latency (if timestamps available)

---

## Analysis Plan

### Statistical Tests

#### 1. PDR Comparison
- **Test:** One-way ANOVA (3 protocols)
- **Null Hypothesis:** No significant difference in PDR between protocols
- **Significance Level:** α = 0.05
- **Post-hoc:** Tukey HSD for pairwise comparisons

#### 2. Latency Comparison
- **Test:** Kruskal-Wallis (non-parametric, latency may not be normal)
- **Null Hypothesis:** No significant difference in latency distributions
- **Visualization:** CDF plots, box plots

#### 3. Overhead Comparison
- **Test:** Descriptive statistics (mean, std dev)
- **Calculation:** `Overhead Ratio = (Total TX - Delivered) / Delivered`
- **Visualization:** Bar chart with error bars

### Visualization Requirements

#### Required Plots
1. **PDR vs Distance** (line plot with error bars)
2. **Latency CDF** (cumulative distribution per protocol)
3. **Overhead Bar Chart** (normalized per protocol)
4. **Route Stability Timeline** (cost routing vs hop-count)
5. **Cost Evolution** (cost routing only, time series)

#### Optional Plots
- RSSI/SNR heatmap over time
- Packet loss patterns (temporal distribution)
- Network topology diagram with link qualities

---

## Success Criteria

### Minimum Viable Results
- ✅ PDR measured for all 3 protocols (at least 2 runs each)
- ✅ Latency data collected (at least 100 samples per protocol)
- ✅ Overhead calculated and compared
- ✅ Statistical significance tested (p-values reported)

### Ideal Results
- ✅ All 3 scenarios completed
- ✅ 3 repetitions per configuration
- ✅ Multi-distance data collected
- ✅ Cost routing demonstrates measurable improvement in challenging conditions

### Expected Findings
1. **Flooding:** High overhead, high PDR, moderate latency
2. **Hop-Count:** Low overhead, high PDR, low latency
3. **Cost Routing:** Similar to hop-count with potential advantages:
   - Better PDR in poor link conditions
   - Route adaptation visible in logs
   - Link quality awareness demonstrated

---

## Timeline Estimate

### Week 6 (Implementation)
- Day 1-2: Data collection scripts (2 days)
- Day 3: Analysis environment setup (1 day)
- Day 4-5: Test execution - Flooding + Hop-Count (2 days)

### Week 7 (Testing & Analysis)
- Day 1-2: Test execution - Cost Routing (2 days)
- Day 3-4: Data analysis and statistical tests (2 days)
- Day 5: Documentation and visualization (1 day)

**Total Estimated Time:** 10 days

---

## Risk Mitigation

### Potential Issues

1. **Timestamp Synchronization**
   - **Risk:** Nodes not synchronized, latency calculation inaccurate
   - **Mitigation:** Use relative timestamps, or manual sync via NTP before test
   - **Fallback:** Report hop-count as proxy for latency

2. **Insufficient Test Duration**
   - **Risk:** Not enough samples for statistical significance
   - **Mitigation:** Extend test duration to 45-60 minutes if needed
   - **Fallback:** Reduce confidence level, acknowledge limitation

3. **Environmental Interference**
   - **Risk:** External 915 MHz interference affects results
   - **Mitigation:** Test in controlled environment, multiple time slots
   - **Fallback:** Document interference, use relative comparisons

4. **Hardware Failures**
   - **Risk:** Node crashes or battery dies mid-test
   - **Mitigation:** Fresh batteries, monitor all nodes, restart on failure
   - **Fallback:** Exclude failed runs, document incident

5. **Cost Routing Shows No Benefit**
   - **Risk:** In good conditions, cost routing = hop-count
   - **Mitigation:** Test in challenging scenarios (longer distance, obstacles)
   - **Interpretation:** Document that cost routing provides *safety* without penalty

---

## Deliverables

### Documentation
- [ ] This protocol document (experiments/configs/week6_7_protocol.md)
- [ ] Data collection scripts (utilities/data_collection.py)
- [ ] Analysis notebook (analysis/week6_7_performance.ipynb)
- [ ] Results summary (experiments/results/SUMMARY.md)

### Data Files
- [ ] Raw CSV logs (experiments/results/[protocol]/run[N].csv)
- [ ] Processed data (experiments/results/processed/)
- [ ] Plots and figures (experiments/results/figures/)

### Final Report
- [ ] Statistical test results with p-values
- [ ] Comparison tables (mean ± std dev for all metrics)
- [ ] Visualization suite (all required plots)
- [ ] Discussion of findings (thesis-ready)
- [ ] Updated CHANGELOG with Week 6-7 entry

---

## Notes

### Assumptions
1. Sensor transmits every 30 seconds (as implemented in firmware)
2. HELLO interval is 120 seconds (LoRaMesher default)
3. TX power is 10 dBm (testing mode) or -3 dBm (production)
4. LoRa config: SF7, BW125, CR 4/7, 915 MHz (consistent across all tests)

### Constraints
- Limited to 3 hardware nodes
- Indoor testing environment (lab or office)
- Manual data collection (no automated test harness)
- Time budget: ~10 days for complete evaluation

### Future Extensions (Out of Scope for Week 6-7)
- Multi-gateway testing
- Mobile nodes (topology changes during test)
- Larger mesh (5+ nodes)
- Long-duration tests (24+ hours)
- Outdoor/longer-range testing

---

**Status:** Protocol defined, ready for implementation
**Next Steps:** Create data collection scripts (Task 2)
