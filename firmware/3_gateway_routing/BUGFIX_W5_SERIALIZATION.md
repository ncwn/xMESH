# BUGFIX: W5 Serialization Drift

Last updated: 2025-11-15

## Problem Statement

Earlier iterations attempted to use a local `gatewayLoads[]` array to drive W5. Each gateway knew only its own packet count, so `numGateways` never exceeded 1 and the bias always returned 0. Even after multi-gateway discovery worked, no node knew the other gateways' load. The missing piece was a serialization channel.

## Fix Summary

1. **Extended HELLO payload:**
   - Added `uint8_t gatewayLoad` to `RoutePacket`.
   - Added `uint8_t gatewayLoad` to `NetworkNode`.
   - Default value `255` means "unknown". Valid loads occupy `[0, 254]` and represent packets/minute.

2. **Library updates (LoRaMesher core):**
   - `PacketService::createRoutingPacket()` now initialises `gatewayLoad = 255`.
   - `RoutingTableService::processRoute()` copies encoded load for both the sender (`RoutePacket.gatewayLoad`) and every `NetworkNode` entry when the value is ≠255.
   - `RouteNode` constructor copies the entire `NetworkNode` struct to prevent metadata loss.

3. **Firmware integration (Protocol 3):**
   - Trickle HELLO task samples gateway load before queueing packets and stamps the header.
   - Routing cost callback extracts gateway loads directly from the routing table instead of the old `gatewayLoads[]` array.

## Compatibility

- **Backward**: Nodes without the new field still transmit `gatewayLoad = 255` (unknown). New nodes simply treat it as "no data" and skip W5.
- **Forward**: Because both firmware and the library live in this repository, updating `file://../..` in `platformio.ini` keeps builds consistent. External projects must update their LoRaMesher dependency to a commit that contains the new struct layout.
- **Packet Size**: Added 1 byte to both the HELLO header and each routing entry. Even with the new field, the maximum node count per HELLO remains within SX1262 MTU constraints.

## Validation Checklist

- [x] Build succeeds for Protocol 3 (`pio run`).
- [x] Routing table dump shows non-zero `gatewayLoad` for gateways after at least one HELLO.
- [x] Sensors log `[W5]` messages when two gateways report different loads.
- [x] Regression smoke for Protocol 1 & 2 (Nov 10–12 indoor reruns already used the forked LoRaMesher build with the new byte; no parsing errors observed).

## Notes

- If a third-party project includes precompiled LoRaMesher binaries, they must be rebuilt to include the new field; otherwise, HELLO parsing will reject packets (size mismatch).
- Keep this document synced with `W5_LOAD_SHARING_DESIGN.md` whenever the encoding scheme changes.
