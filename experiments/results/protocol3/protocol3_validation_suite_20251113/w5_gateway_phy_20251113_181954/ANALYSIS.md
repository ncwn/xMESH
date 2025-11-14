# w5_gateway_phy_20251113_181954 — Physical Multi-Hop Validation

**Date:** November 13, 2025
**Topology (based on comprehensive log analysis):**
- **Gateways:** BB94 (node2 log), 8154 (node1 log)
- **Relay:** 6674 (confirmed from relay log showing "Node 6674 (RELAY)")
- **Sensors:** D218 (sensor with direct connectivity to gateways), 02B4 (sensor with direct connectivity to gateway BB94)

**Important Discovery:** While multi-hop routes exist in routing tables, actual data packets show Hops=0 (direct delivery), indicating sensors maintained direct connectivity despite weak RSSI

**Transmit Power:** 4 dBm on the far sensor + relay, 7 dBm on the near sensor, 10 dBm on both gateways.  
**Duration:** ~10 minute field shakedown (full 30 min capture forthcoming once spacing stays stable).

## Evidence Summary

1. **Multi-Hop Routes Exist in Routing Tables (But Not Used for Data)**
   - `node2_20251113_181954.log` (Gateway BB94) routing table shows:
     ```
     [18:20:28.756] D218 | D218 | 1 | 00 | 1.47  (D218 direct, 1 hop)
     [18:20:28.759] 6674 | D218 | 2 | 00 | 2.47  (Relay 6674 via D218, 2 hops)
     [18:20:28.762] 8154 | D218 | 3 | 01 | 3.47  (Gateway 8154 via D218, 3 hops)
     ```
     Format: `Destination | Via | Hops | Role | Cost`
   - `node1_20251113_181954.log` (Gateway 8154) routing table shows:
     ```
     [18:22:00.722] D218 | D218 | 1 | 00 | 1.47  (D218 direct, 1 hop)
     [18:22:00.725] 6674 | D218 | 2 | 00 | 2.47  (Relay 6674 via D218, 2 hops)
     ```
   - **Key Finding**: Both gateways have multi-hop routes to relay 6674 (via D218), but actual data packets from D218 arrive with Hops=0 (direct transmission)

2. **Actual Data Transmission Pattern**
   - Gateway 8154 received 6 data packets from D218 (Seq 1-6) all with **Hops=0** (direct transmission)
   - Gateway BB94 received 7 packets from 02B4 (Seq 15-22) and 2 from D218 (Seq 7-8) all with **Hops=0**
   - **Critical Finding**: Despite multi-hop routes in tables, all data packets were delivered directly without relay forwarding

3. **Adaptive Cost & Trickle Behavior**
   - Gateway BB94 shows route switching at `18:20:49`: Route to 8154 changed from `via D218` (3 hops) to direct `via 8154` (1 hop) as link improved
   - Relay logs (18:06-18:10) show Trickle adaptation: suppression efficiency 33.3%, interval increased from 60s to 240s
   - Both behaviors confirm the cost-based routing and Trickle mechanisms are functioning

4. **Relay Forwarding Analysis**
   - **Relay Log Evidence (18:06-18:10)**: Shows `FWD: 0` throughout, confirming no packet forwarding occurred
   - Relay routing table shows direct connections: D218 (1 hop) and gateway 8154 (1 hop)
   - Error at 18:10:42: `[E] NextHop Not found from 2B4, destination BB94` - relay couldn't find route for 02B4→BB94
   - **Conclusion**: The relay was not positioned to provide multi-hop connectivity; all nodes maintained direct connections

## Remaining Actions

- **Critical**: Increase node separation or reduce TX power further (2 dBm or less) to force true multi-hop routing
- Deploy nodes with sufficient physical distance to prevent direct sensor-to-gateway communication
- Capture logs from all nodes simultaneously during the 30-minute validation run
- Verify relay actually forwards packets (FWD counter should increment)

## Key Findings & Issues

1. **Multi-hop Not Achieved**: Despite 4-10 dBm TX power settings, all data packets were delivered directly (Hops=0)
2. **Routing Table vs Reality**: Multi-hop routes exist in routing tables but aren't used for actual data transmission
3. **Node Identities Confirmed**:
   - 6674 = Relay (from relay log heartbeat messages)
   - D218 = Sensor (generates data packets)
   - 02B4 = Sensor (generates data packets)
   - BB94, 8154 = Gateways
4. **RSSI Values**: Relay shows weak links (-117 to -125 dBm) but still sufficient for direct communication
5. **Test Configuration Issue**: Current physical setup doesn't enforce multi-hop requirement; nodes too close or TX power too high
