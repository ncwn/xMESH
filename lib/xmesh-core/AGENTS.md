# lib/xmesh-core - Advanced Routing

## PURPOSE

Multi-metric cost routing extensions for LoRaMesher. Zero-overhead link quality tracking and adaptive HELLO scheduling.

## MODULES

| File | Lines | Purpose |
|------|-------|---------|
| `CostRouter.cpp` | 60 | Multi-metric cost: `cost = W1*hops + W2*(1-RSSI) + W3*(1-SNR) + W4*ETX + W5*gatewayBias` |
| `ETXTracker.cpp` | 180 | Sequence-gap ETX: detects loss via missing seq nums, no ACKs needed |
| `TrickleScheduler.cpp` | 190 | RFC 6206: adaptive HELLO intervals (60s→600s), suppression counter |
| `GatewayBalancer.cpp` | 270 | Load-biased gateway selection, fast neighbor failure detection (360s) |
| `MobilityDetector.cpp` | 160 | SNR variance → mobility state → adaptive params |
| `RoutingAdapter.cpp` | 50 | Thread-safe `RouteNodeCopy` snapshot for decoupled queries |

## INTEGRATION

All modules instantiated in `firmware/production/src/main.cpp`:
```cpp
xmesh::TrickleScheduler trickle(TRICKLE_I_MIN, TRICKLE_I_MAX, TRICKLE_K, TRICKLE_ENABLED);
xmesh::CostRouter costRouter(W1_HOP_COUNT, W2_RSSI, W3_SNR, W4_ETX, W5_GATEWAY_BIAS);
xmesh::ETXTracker etxTracker;
xmesh::GatewayBalancer gatewayBalancer;
xmesh::MobilityDetector mobilityDetector;
```

Hook into LoRaMesher via:
```cpp
RoutingTableService::setCostCalculationCallback(costCalculationCallback);
RoutingTableService::setHelloReceivedCallback(helloReceivedCallback);
```

## KEY TYPES

- `xmesh::LinkMetrics` - RSSI, SNR, ETX, sliding window
- `xmesh::RouteNodeCopy` - Thread-safe snapshot of RouteNode
- `xmesh::GatewayLoadState` - packets/min tracking
- `xmesh::NeighborHealth` - failure detection state

## CONFIGURATION

All weights/thresholds in `firmware/production/include/config.h`:
- `W1_HOP_COUNT`, `W2_RSSI`, `W3_SNR`, `W4_ETX`, `W5_GATEWAY_BIAS`
- `TRICKLE_I_MIN`, `TRICKLE_I_MAX`, `TRICKLE_K`
- `DETECTION_THRESHOLD_MS`, `WARNING_THRESHOLD_MS`

## TESTS

`firmware/production/test/test_native/` contains Unity tests for each module.
