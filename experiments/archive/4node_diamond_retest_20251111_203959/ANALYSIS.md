# Test Analysis: Protocol 3 - 4-Node Diamond Topology (Retest)

**Test Date:** November 11, 2025, 20:39-21:09
**Duration:** 30 minutes
**Transmit Power:** 10 dBm
**Topology:** Diamond 4-node (Sensor with dual relay paths to gateway)
**Test Environment:** Indoor table-top, 4m radius
**Monitoring:** Gateway + Relay + Sensor (3 of 4 nodes captured)
**Branch:** xMESH-Test

**Note:** This retest was conducted after initial diamond test (4node_diamond_20251111_194847) failed due to sensor not transmitting. This test included sensor monitoring to verify transmission.

---

## Test Configuration

### Hardware Setup

| Log File | Node ID | Role | Address | Monitoring Status |
|----------|---------|------|---------|-------------------|
| node1_*.log | 4 | Relay | 02B4 | USB serial capture |
| node2_*.log | 5 | Gateway | BB94 | USB serial capture |
| node3_*.log | 1 | Sensor | D218 | USB serial capture |

**Physical Layout:**
```
      [Relay3]
     /        \
 [Sensor1]  [Gateway5]
     \        /
      [Relay4] ← Monitored
```

Sensor has two potential paths to gateway through either relay node.

**Data Collection:**
All three monitored nodes captured complete event logs, enabling verification of sensor transmission, relay forwarding, and gateway reception.

---

## Packet Delivery Ratio Analysis

### Measured Performance

| Metric | Count | Result |
|--------|-------|--------|
| Sensor TX (measured) | 27 packets | Sequences 2-28 |
| Gateway RX (measured) | 27 packets | All sequences received |
| PDR | 27/27 | 100% |
| Packet Loss | 0 | None |

**Sequence Pattern:**
- Sequences 0-1: Not transmitted (route discovery phase, ~3 minutes)
- Sequences 2-28: All transmitted and received successfully
- No gaps in sequence numbers

**Result:** 100% packet delivery ratio for all transmitted packets.

### Comparison with Baseline Protocols (Diamond Topology)

| Protocol | PDR | Packets | Packet Loss |
|----------|-----|---------|-------------|
| Protocol 1 (Flooding) | 96.7% | 29/30 | 1 packet |
| Protocol 2 (Hop-Count) | 96.7% | 29/30 | 1 packet |
| **Protocol 3 (Proposed)** | **100%** | **27/27** | **0** |

Protocol 3 achieved perfect delivery (100% PDR) compared to 96.7% for baseline protocols. The difference of 3.3 percentage points is modest but represents zero packet loss versus one lost packet in baseline tests.

---

## Control Overhead Analysis

### Gateway HELLO Activity (Directly Measured)

From gateway logs:
- Trickle HELLOs transmitted: 2
- Safety HELLOs transmitted: 9
- HELLO transmissions suppressed: 5
- Suppression efficiency: 71.4% (5 of 7 opportunities)

### Network-Wide Overhead (Estimated)

**Estimation Methodology:**
Based on gateway HELLO pattern (11 HELLOs per node), estimated for 4-node network:

- Estimated total: 4 nodes × 11 HELLOs = **~44 HELLOs**
- Protocol 2 baseline: 4 nodes × 15 HELLOs (120s interval) = **60 HELLOs**
- Estimated reduction: (60 - 44) / 60 = **26.7%**

**Confidence:** Medium - extrapolated from single node observation

**Limitation:** Relay and sensor HELLO behavior not directly observed. Assumes symmetric Trickle operation across all node types per firmware design.

---

## Trickle Scheduler Performance

### Observed Characteristics

**Interval Progression:**
- Started at I_min = 60 seconds
- Progressively doubled during stable network operation
- Reached I_max = 600 seconds
- Maintained maximum interval throughout test

**Suppression Performance:**
- Gateway suppressed 5 of 7 potential HELLO transmissions (71.4%)
- Indicates redundancy detection functioning correctly
- Neighbors' HELLOs provided sufficient network information

**Stability Indicators:**
- No false fault detections observed
- No route timeouts recorded
- Minimal Trickle resets after initial convergence

The Safety HELLO mechanism (180-second interval) transmitted 9 times during the test, consistent with the configured safety period (30 min / 3 min = 10 intervals with startup offset).

---

## Topology-Specific Observations

### Diamond vs Linear Performance (Protocol 3)

| Topology | Sensors | PDR | Packet Loss |
|----------|---------|-----|-------------|
| Linear (4-node) | 2 | 96.7% | 2/60 (3.3%) |
| Diamond (4-node) | 1 | 100% | 0/27 (0%) |

**Observation:** Single-sensor diamond topology achieved perfect delivery, while dual-sensor linear topology experienced minimal packet loss. This pattern is consistent across all three protocols and likely reflects collision probability increasing with the number of simultaneous transmitters.

---

## Monitoring Configuration

**Nodes Captured:**
- Gateway: Complete packet reception, Trickle state, routing decisions
- Relay: Forwarding activity logs
- Sensor: Transmission confirmation, routing table evolution

**Data Quality:**
- High confidence: PDR (direct count of gateway reception)
- High confidence: Sensor TX verification (captured sensor logs)
- Medium confidence: HELLO overhead (gateway measurement extrapolated)

**Advantage over previous attempt:** Sensor monitoring enabled direct verification of packet transmission, confirming sensor TX task operation and ruling out sensor-side issues.

---

## Test Conditions

**Physical Environment:**
- 4 nodes within 4m radius
- Indoor table-top deployment
- Line-of-sight between all nodes
- 10 dBm transmit power

**Routing Behavior:**
All nodes appear to be in direct communication range (hops=1 in routing tables). The dual relay paths in diamond topology provide redundancy but were not required for packet delivery given the direct connectivity at 10 dBm power level.

**Environmental Note:** Dense placement with high power creates a stress test for collision management rather than a typical mesh deployment scenario.

---

## Summary of Findings

**Packet Delivery:** 100% PDR achieved (27/27 packets), representing perfect delivery for all transmitted sequences.

**Control Overhead:** Gateway exhibited 11 HELLO transmissions over 30 minutes. Extrapolating to network-wide overhead suggests approximately 27% reduction compared to fixed 120-second HELLO intervals.

**Network Stability:** No fault detection anomalies, route timeouts, or protocol instability observed.

**Limitation:** Multi-hop routing behavior not validated due to direct path connectivity. Physical separation testing recommended to validate relay-mediated communication and cost-based path selection between multiple relay options.

---

**Test Status:** Validated for PDR and overhead; route selection behavior pending physical deployment scenarios with forced multi-hop topologies.
