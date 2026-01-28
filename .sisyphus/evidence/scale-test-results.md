# xMESH Scale Testing Plan (5-10 Nodes)

## Overview
This document provides a comprehensive test plan for evaluating the scalability and performance of xMESH in deployments of 5 to 10 nodes. Since physical hardware is not available in the current environment, this plan serves as a rigorous guide for user execution.

The goal of scale testing is to verify that the adaptive Trickle scheduling, multi-metric cost routing, and zero-overhead ETX tracking maintain network efficiency and reliability as the node density and hop count increase.

## Test Topologies

### 5-Node Topology (Linear/Chain)
Used to test multi-hop relaying and ETX accumulation over distance.
```
[GW] <--- (Link 1) ---> [N1] <--- (Link 2) ---> [N2] <--- (Link 3) ---> [N3] <--- (Link 4) ---> [N4]
```
*   **GW**: Gateway Node (connected to WiFi/Serial)
*   **N1-N4**: Mesh Nodes
*   **Purpose**: Verify that N4 can reliably reach GW through 4 hops and that CostRouter selects the most stable chain.

### 5-Node Topology (Star/Cluster)
Used to test Trickle suppression in high-density environments.
```
      [N1]
       |
[N2]--[GW]--[N3]
       |
      [N4]
```
*   **Purpose**: Maximize HELLO suppression. Since all nodes hear each other, Trickle should significantly reduce control traffic.

### 10-Node Topology (Multi-Hop Grid)
Used to test complex routing decisions and load balancing.
```
[GW1] ---- [N1] ---- [N2]
  |         |         |
[N3] ---- [N4] ---- [N5]
  |         |         |
[N6] ---- [N7] ---- [GW2]
```
*   **GW1, GW2**: Gateway Nodes
*   **N1-N7**: Mesh Nodes
*   **Purpose**: Test GatewayBalancer load sharing and CostRouter path selection when multiple gateways are available.

## Expected Performance Metrics
Based on "Protocol 3" research baselines, the following metrics are expected in a stable environment:

| Metric | Target Value | Description |
| :--- | :--- | :--- |
| **PDR** | 96% - 100% | Packet Delivery Ratio for application data |
| **HELLO Reduction** | 30% - 45% | Reduction in control traffic via Trickle suppression |
| **ETX Convergence** | 5 - 10 Intervals | Time for ETX to stabilize after node movement/boot |
| **Route Stability** | < 1 Flap/Hour | Number of route changes in a static environment |
| **Convergence Time** | < 120 Seconds | Time for all 10 nodes to find a path to a Gateway |

## Test Procedure

### 1. Hardware Setup
1.  Prepare 5 or 10 Heltec WiFi LoRa 32 V3 devices.
2.  Assign unique MAC addresses/Node IDs (handled automatically by LoRaMesher).
3.  Designate 1 or 2 nodes as Gateways (connected to power and optionally WiFi).
4.  Position nodes according to the chosen topology diagrams.

### 2. Firmware Flashing
For each node, flash the production firmware:
```bash
cd firmware/production
pio run -t upload
```

### 3. Data Collection
1.  Connect serial monitors to at least the Gateway nodes and the furthest leaf nodes.
2.  Log serial output to text files:
    ```bash
    pio device monitor --baud 115200 | tee node_log.txt
    ```
3.  Run the test for a minimum of **1 hour** to allow Trickle intervals to reach `I_max` (600s).

### 4. Analysis Methodology
Use the following commands to parse logs for key metrics:

**Trickle Suppression Ratio:**
```bash
# Count Transmissions vs Suppressions
grep "TRANSMIT" node_log.txt | wc -l
grep "SUPPRESS" node_log.txt | wc -l
```

**ETX Stability:**
```bash
# Extract ETX values for a specific link
grep "ETX updated for [NODE_ID]" node_log.txt
```

**Routing Table Changes:**
```bash
# Monitor cost evaluations
grep "Cost evaluation" node_log.txt
```

## Metrics to Collect

### 1. Packet Delivery Ratio (PDR)
*   **Method**: Send 100 packets from the furthest node to the Gateway at 30s intervals.
*   **Formula**: `(Received Packets / Sent Packets) * 100`

### 2. HELLO Overhead Reduction
*   **Method**: Compare `transmitCount` vs `suppressCount` from `TrickleScheduler`.
*   **Formula**: `suppressCount / (transmitCount + suppressCount)`

### 3. ETX Convergence Time
*   **Method**: Measure time from node boot until the `ETX updated` log shows a stable value (variance < 0.1).

### 4. Memory Footprint
*   **Method**: Monitor `[HEALTH] Free heap` logs.
*   **Target**: Heap should remain > 15KB and stable (no leaks).

## Pass Criteria
1.  **Connectivity**: 100% of nodes must establish a route to at least one Gateway.
2.  **Reliability**: PDR > 95% for 1-hop links, > 90% for 4+ hop links.
3.  **Efficiency**: Trickle suppression > 20% in the 10-node grid.
4.  **Stability**: No watchdog resets or heap exhaustion during the 1-hour test.

## Troubleshooting

| Issue | Potential Cause | Solution |
| :--- | :--- | :--- |
| **Node not joining** | LoRa interference or range | Increase SF or check antenna connections. |
| **High ETX (> 5.0)** | High packet loss or collisions | Check for overlapping HELLO intervals or physical obstacles. |
| **No Suppression** | Trickle `k` value too high or hidden nodes | Verify nodes can hear each other's HELLOs; check `config.h`. |
| **Frequent Route Flapping** | Similar costs for multiple paths | Increase hysteresis in `LoRaMesher` cost comparison. |

---
**Note**: This test plan is based on the xMESH Production Refactor v1.0. Actual results may vary based on environmental factors (RF noise, physical obstructions).
