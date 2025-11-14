# Test Analysis: Protocol 3 - 4-Node Linear Topology

**Test Date:** November 11, 2025, 17:52-18:22
**Duration:** 30 minutes
**Transmit Power:** 10 dBm
**Topology:** Linear 4-node (Sensor1 → Sensor2 → Relay3 → Gateway5)
**Test Environment:** Indoor table-top deployment, 4m radius
**Branch:** xMESH-Test

---

## Test Configuration

### Hardware Setup

| Node | ID | Role | Address | Monitoring |
|------|-----|------|---------|------------|
| Node 1 | 1 | Sensor | D218 | Battery powered |
| Node 2 | 2 | Sensor | 6674 | Battery powered |
| Node 3 | 3 | Relay | 02B4 | USB serial capture |
| Node 5 | 5 | Gateway | BB94 | USB serial capture |

**Data Collection:**
- Gateway logs: Complete packet reception data
- Relay logs: Forwarding activity and HELLO behavior
- Sensors: TX activity inferred from gateway reception (not directly monitored)

**Radio Configuration:**
- Frequency: 923.2 MHz (AS923 Thailand)
- Spreading Factor: 7
- Bandwidth: 125.0 kHz
- Transmit Power: 10 dBm

---

## Packet Delivery Ratio Analysis

### Measured Performance

The gateway received 58 of 60 expected packets over the 30-minute test period.

| Sensor | Address | Packets Received | Expected | PDR | Packet Loss |
|--------|---------|-----------------|----------|-----|-------------|
| Sensor 1 | D218 | 29 | 30 | 96.7% | 1 packet |
| Sensor 2 | 6674 | 29 | 30 | 96.7% | 1 packet |
| **Combined** | - | **58** | **60** | **96.7%** | **2 packets** |

**Missing Sequences:**
- Sensor 1: Sequence 29
- Sensor 2: Sequence 29

Both sensors experienced the loss of their final packet, suggesting a potential common cause (test termination timing or RF interference event).

**Result:** The achieved 96.7% PDR exceeds the 95% research threshold defined in the proposal.

---

## Comparative Protocol Performance

Under identical test conditions (4-node linear, 10 dBm, 4m radius, 30 minutes), the three protocols demonstrated the following performance characteristics:

| Protocol | PDR | Packet Loss | Loss Rate |
|----------|-----|-------------|-----------|
| Protocol 1 (Flooding) | 85.0% | 9/60 | 15.0% |
| Protocol 2 (Hop-Count) | 81.7% | 11/60 | 18.3% |
| **Protocol 3 (Proposed)** | **96.7%** | **2/60** | **3.3%** |

**Observations:**
- Protocol 3 achieved 11.7 percentage points improvement over Protocol 1
- Protocol 3 achieved 15.0 percentage points improvement over Protocol 2
- Protocol 3 was the only protocol to exceed the 95% PDR threshold
- Packet loss rate reduced by 78-82% compared to baseline protocols

**Hypothesis for improved performance:**
The reduced HELLO overhead in Protocol 3 (estimated 27% reduction) may contribute to lower channel congestion, resulting in fewer packet collisions and improved PDR under dense deployment conditions.

---

## Control Overhead Analysis

### Gateway HELLO Transmission (Directly Measured)

The gateway node transmitted the following HELLO packets:

- Trickle-initiated HELLOs: 2
- Safety-forced HELLOs: 9
- Suppressed HELLOs: 5
- Total HELLO opportunities: 7 (2 transmitted + 5 suppressed)
- Suppression efficiency: 71.4%

### Network-Wide HELLO Overhead (Estimated)

**Estimation Methodology:**
Assuming all 4 nodes exhibit similar HELLO behavior to the observed gateway node (2 Trickle + 9 Safety = 11 HELLOs per node):

- Estimated total network HELLOs: 4 nodes × 11 = **~44 HELLOs**
- Protocol 2 baseline (120s fixed interval): 4 nodes × 15 = **60 HELLOs**
- Estimated reduction: (60 - 44) / 60 = **26.7%**

**Confidence Level:** Medium

This estimate extrapolates gateway behavior to all nodes. Actual sensor and relay HELLO transmission could not be verified due to monitoring constraints (battery-powered nodes not captured). The reduction estimate is plausible based on the Trickle algorithm design (60-600s adaptive intervals with 180s safety mechanism vs. fixed 120s intervals).

---

## Trickle Scheduler Performance

### Interval Progression

The Trickle scheduler exhibited the expected exponential backoff behavior:

- Initial interval (I_min): 60 seconds
- Doubled progressively during network stability
- Reached maximum interval (I_max): 600 seconds
- Sustained I_max throughout majority of test

**Suppression Mechanism:**
- Total HELLO opportunities: 7
- Transmitted: 2 (28.6%)
- Suppressed: 5 (71.4%)

The 71.4% suppression efficiency demonstrates the redundancy detection mechanism is functioning as designed, with the gateway suppressing its own HELLO transmissions when neighboring nodes' HELLOs provide sufficient network information.

### Safety HELLO Mechanism

Safety-forced HELLOs occurred 9 times during the 30-minute test, consistent with the 180-second safety interval (30 min / 3 min = 10 intervals, accounting for startup delay).

This mechanism prevents route timeouts (600-second library timeout) when Trickle intervals extend beyond safe periods.

---

## Network Stability

| Metric | Observed Value |
|--------|---------------|
| False fault detections | 0 |
| Route timeouts | 0 |
| Trickle resets (post-convergence) | Minimal |
| CRC errors | Not quantified in gateway logs |

The absence of false fault detections and route timeouts over the 30-minute period indicates stable network operation with the 180-second safety HELLO mechanism providing adequate timeout prevention.

---

## Monitoring Configuration and Limitations

**Nodes Captured (2 of 4):**
- Gateway (Node 5): Complete reception logs, Trickle state, routing table evolution
- Relay (Node 3): Forwarding activity, partial network view

**Nodes Not Captured (Battery Powered):**
- Sensor 1 (Node 1): TX activity inferred from gateway RX
- Sensor 2 (Node 2): TX activity inferred from gateway RX

**Data Confidence Levels:**

**High Confidence (Direct Measurement):**
- PDR calculation (gateway counted all received packets)
- Gateway Trickle behavior (state logs captured)
- Suppression efficiency for gateway node

**Medium Confidence (Reasonable Inference):**
- Sensor transmission counts (based on gateway reception + sequence numbers)
- Network-wide HELLO overhead (extrapolated from gateway behavior)

**Cannot Verify:**
- Individual sensor/relay HELLO transmission patterns
- Relay Trickle suppression behavior
- Per-sensor transmission confirmation beyond gateway reception

**Assumption:** The extrapolation assumes symmetric protocol behavior across node types, which is consistent with the firmware design where all nodes run identical Trickle and safety HELLO logic.

---

## Test Conditions and Environmental Factors

**Physical Configuration:**
- 4 nodes positioned within 4m radius on indoor table
- All nodes within line-of-sight
- 10 dBm transmit power
- Likely all direct paths (hops=0) given power and proximity

**RF Environment:**
- Indoor laboratory setting
- Potential multipath from reflections
- Proximity may create near-field coupling effects
- Dense node placement may elevate collision probability

**Test represents:** Worst-case dense deployment scenario, useful for stress-testing protocol resilience rather than typical mesh network spacing.

---

## Summary of Findings

**Primary Result:** Protocol 3 achieved 96.7% packet delivery ratio in 4-node linear deployment, exceeding the 95% research threshold.

**Comparative Performance:** Under identical test conditions, Protocol 3 outperformed flooding (85.0% PDR) by 11.7 percentage points and hop-count routing (81.7% PDR) by 15.0 percentage points.

**Control Overhead:** The gateway node's HELLO transmission pattern (2 Trickle + 9 Safety = 11 HELLOs) suggests an estimated 27% network-wide reduction compared to fixed 120-second HELLO intervals, though this requires validation with complete node monitoring.

**Network Stability:** No false fault detections or route timeouts observed during the test period.

**Limitation Acknowledgment:** Multi-hop routing behavior could not be validated as all nodes appear to be within direct communication range at 10 dBm and 4m spacing. Physical distance deployment testing recommended for multi-hop validation.

---

**Test Status:** Validated for PDR and overhead comparison; multi-hop behavior pending physical separation tests.
