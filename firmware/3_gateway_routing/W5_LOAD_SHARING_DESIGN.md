# W5 Load Sharing Design

Last updated: 2025-11-15

## Objective

Activate the W5 gateway bias term without introducing a new control packet. Gateways must share their processing load with every sensor so that route selection can penalize congested gateways and favor lighter ones.

### Success Criteria

1. **Zero extra packet types** – piggyback on the existing HELLO/RoutePacket flow.
2. **Multi-hop propagation** – sensors that cannot hear a gateway directly must still learn its load within two HELLO cycles.
3. **Deterministic bias** – `calculateGatewayBias()` behaves deterministically: `(load - avg) / avg`.
4. **Observable telemetry** – logs expose `[W5] Gateway XXXX load=Y avg=Z bias=B` whenever the bias impacts selection.

## Architecture Overview

| Stage | Component | Purpose |
| --- | --- | --- |
| 1 | `recordGatewayLoadSample()` | Count packets processed by a gateway between HELLOs. |
| 2 | `sampleLocalGatewayLoadForHello()` | Convert packet counts to packets/minute and encode into a single byte (0-254). |
| 3 | `RoutePacket.gatewayLoad` | HELLO header carries the sender's encoded load (255 = unknown). |
| 4 | `NetworkNode.gatewayLoad` | Routing table stores encoded load per gateway entry, enabling propagation when other nodes broadcast HELLO. |
| 5 | `calculateGatewayBias()` | Iterates routing table, computes average load for all gateways with data, and returns `(thisLoad - avg) / avg`. |

## Detailed Flow

1. **Measurement (gateway-only):**
   - Every RX data packet increments `localGatewayLoadState.packetsSinceLastSample`.
   - During HELLO transmission, the gateway computes packets/minute over the elapsed window, encodes it, and resets the counter.

2. **Serialization:**
   - `RoutePacket` gained a `gatewayLoad` byte.
   - `NetworkNode` gained a `gatewayLoad` byte.
   - Trickle HELLO task sets `RoutePacket.gatewayLoad` before enqueueing.

3. **Propagation:**
   - `RoutingTableService::processRoute()` copies the encoded load whenever a HELLO contains valid data (≠255).
   - Downstream nodes automatically forward this metadata because `getAllNetworkNodes()` copies the full `NetworkNode` struct.

4. **Cost Evaluation:**
   - Protocol 3 now inspects the routing table to gather all gateways with valid load.
   - If at least two gateways have recent data and the average load ≥0.2 pkt/min, the bias term becomes active.
   - Debug logs fire whenever `|bias| > 0.01`.

5. **Failure Modes:**
   - Missing data → encoded value stays 255, meaning "unknown" (bias defaults to 0).
   - Radio silence → encoded load decays to 0 pkt/min, which results in a negative bias (favors the idle gateway).

## Configuration Touchpoints

| File | Setting | Description |
| --- | --- | --- |
| `src/main.cpp` | `MIN_GATEWAY_LOAD_WINDOW_MS` | Minimum time window (ms) used when sampling packets/minute. Prevents divide-by-zero for back-to-back HELLOs. |
| `src/main.cpp` | `MIN_GATEWAY_LOAD_FOR_BIAS` | Average load threshold before W5 becomes active. Shields against noise when traffic is extremely low. |
| `src/config.h` | `W5_GATEWAY_BIAS` | Scaling factor applied to the bias term. Default 1.0 continues to work. |

## Telemetry & Debugging

1. `[W5] Gateway XXXX load=Y avg=Z bias=B` – emitted whenever bias magnitude exceeds 0.01.
2. `peekLocalGatewayLoad()` can be used on OLED/debug pages to display the gateway's own load sample.
3. To confirm propagation, inspect routing table dumps – gateway entries now include the encoded load byte.

### Bug Observation (w5_test_20251113_000436 & w5_test_20251113_001608)

- **Symptom:** Even with `[W5]` logs showing BB94 biasing positive and 8154 negative, the sensor continued to transmit to BB94 (`node3_20251113_000436.log:00:09:45.089` and `node3_20251113_001608.log:00:20:16.332`).
- **Root Cause:** `radio.getClosestGateway()` relies on `RoutingTableService::getBestNodeByRole()`, which still compared hop count only. Our first attempt to call the cost callback from inside that function invoked it while the routing-table mutex was held, so the callback bailed before the W5 logic could execute.
- **Fix (Nov 15 2025):** `getBestNodeByRole()` now snapshots gateway candidates under the lock, releases it, evaluates the cost callback per candidate, and then returns the node matching the lowest cost. Gateway choice now honors the same cost function used during route installation, so W5 bias directly affects `radio.getClosestGateway()`.

### Fix Follow-Up (w5_test_20251113_005414)

- **Observation:** Even with the improved selection logic, real-world tests showed the sensor continuing to use Gateway BB94 because both gateways had identical hop/link costs and LoRaMesher still preferred the first entry before W5 penalties propagated.
- **Solution (Nov 15 2025):** Sensors now apply a **load-biased override** before falling back to the cost-based selector. When at least two gateways report valid load data and the lightest gateway is ≥0.25 pkt/min below the next candidate, the firmware explicitly targets that lighter gateway (`getPreferredGateway()` helper). This guarantees that W5 load information directly influences `sendSensorData()` even if the base cost difference is small.

## Test Plan

1. **Indoor 5-node multi-gateway test (existing topology):**
   - Monitor sensor node serial logs.
   - Expect `[W5]` logs once both gateways share load data (typically after ≤2 HELLOs).
   - Induce imbalance by pausing one gateway's RX task (or reducing its duty-cycle) and observe route switch.

2. **Soak Test (≥2 hours):**
   - Ensure encoded loads do not drift or overflow.
   - Verify Trickle suppression does not starve load updates (safety HELLO keeps 3-minute cap).

3. **Edge Case: Disabled Gateway Radio (see `W5_DISABLED_RADIO_BUG.md`):**
   - Confirm the encoded load decays gracefully and does not leave stale bias in the network.

## Validation Evidence (November 13, 2025)

- `w5_gateway_indoor_20251113_013301/node2_20251113_013301.log`: Sensors emitted `[W5] Gateway XXXX load=Y avg=Z bias=±B` every ~60 s and `[W5] Load-biased gateway selection: <gateway>` before each transmission. Gateways accumulated 16 vs 13 packets over 30 minutes, proving active rebalancing.
- `w5_gateway_indoor_over_I600s_node_detection_20251113_120301/node3_20251113_120301.log`: During a 35-minute endurance run the network kept biasing toward the lighter gateway even while Sensor 02B4, Gateway 8154, and Relay 6674 were powered down sequentially. `[FAULT]` entries (silence 375–384 s) paired with `[W5]` logs confirm that load data decays gracefully and the override immediately selects the surviving gateway.
- OLED screenshots and serial dumps show encoded gateway loads propagating through the routing table (`gatewayLoad` byte populated for both 0x0004 and 0x0005).
- No regressions observed in Protocols 1 or 2 after adding the HELLO byte; both rebuilt successfully and passed their indoor retests on Nov 11–12.

## Open Questions / Future Enhancements

- Multi-gateway >2 nodes: evaluate whether averaging should be weighted by hop distance.
- Consider publishing load deltas inside application packets for even faster convergence (optional).
- Provide OTA knob to adjust `MIN_GATEWAY_LOAD_FOR_BIAS` per deployment profile.
