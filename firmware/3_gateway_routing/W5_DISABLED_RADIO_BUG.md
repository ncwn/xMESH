# W5 Disabled Radio Bug

Last updated: 2025-11-15

## Scenario

During endurance tests we occasionally disable a gateway radio (cut power or stop the RX task) to simulate maintenance. Under the old implementation the gateway's `gatewayLoads[]` entry never decayed, so sensors continued to believe the offline gateway was heavily loaded and bias stayed negative long after the node disappeared.

## Expected Behaviour

1. As soon as a gateway stops receiving packets, its `packetsSinceLastSample` counter stays at 0.
2. The next scheduled HELLO encodes `0 pkt/min`, which translates to a strong **negative** bias (prefer the silent gateway because it is idle).
3. After LoRaMesher's timeout removes the gateway from the routing table, the entry disappears and the bias returns to neutral.

## Actual Behaviour (before fix)

- Offline gateway never transmitted another HELLO, so other nodes kept the last non-zero load forever.
- Sensors permanently penalised the offline gateway even once it came back online, creating "W5 hysteresis".

## Mitigation Implemented

- Safety HELLO (180 s) guarantees a HELLO goes out even if Trickle suppresses transmissions, ensuring the zero-load sample is broadcast before the timeout window.
- If the radio is completely powered off (no more HELLOs), the routing entry eventually expires and is deleted, clearing the stale load.

## Reproduction Test

1. Start a 5-node multi-gateway topology; confirm both gateways report non-zero load.
2. Disable Gateway B's radio (power switch or `radio.sleep()` instrumentation).
3. Watch sensor logs:
   - Within 3 minutes, expect a `[W5]` log where Gateway B's load drops towards 0.
   - After `DEFAULT_TIMEOUT` (~600 s), Gateway B disappears from routing tables.
4. Re-enable Gateway B and verify it republishes load + bias returns to neutral unless traffic asymmetry persists.

**Validation Evidence (Nov 13, 2025):** The endurance run `w5_gateway_indoor_over_I600s_node_detection_20251113_120301` followed this procedure by powering down Gateway 8154 at ~12:18 local time. Sensor logs showed `[W5] Gateway 8154 load=0.0 avg=1.0 bias=-1.00` within two safety HELLOs (≤180 s), and the routing table cleared the stale entry after ~600 s before Gateway 8154 rejoined with a fresh non-zero load sample.

## Outstanding Risks

- If a gateway loses power right before a HELLO window, other nodes may not receive the zero-load announcement. We rely on routing timeout to purge stale load.
- Consider augmenting HELLO with a "load age" counter if field tests show stale load persists longer than acceptable.
