# Firmware Structure Learnings

## Dependency Patterns
- `firmware/3_gateway_routing` depends on `firmware/common` via `build_flags = -I ../common` in `platformio.ini`.
- It also depends on the modified LoRaMesher library in the project root via `lib_deps = file://../..`.

## File Duplication and Versioning
- `firmware/3_gateway_routing/lib/` contains near-duplicates of `src/` files. The `lib/` versions appear to be slightly older or baseline versions (e.g., `display_utils.cpp` in `lib/` uses "Gateway" instead of "GW").
- `firmware/3_gateway_routing/src/` contains the most up-to-date versions with specific Protocol 3 enhancements (e.g., MAC address display, user-friendly Node ID).
- `firmware/common/` contains shared utilities like `logging` and `duty_cycle`. `logging.h` in `common/` has `private` members while the version in `3_gateway_routing/src/` has them as `public`, likely for easier debugging or access by the main application.

## LoRaMesher Integration
- The LoRaMesher library in `src/` is a "fork" with specific support for cost-based routing.
- Key integration point: `RoutingTableService::setCostCalculationCallback`.
- `LM_GOD_MODE` is used to access private members of the library for custom implementations like Trickle HELLO.

## Cost Function
- Implemented in `firmware/3_gateway_routing/src/main.cpp` as a callback.
- Incorporates Hop Count, RSSI, SNR, ETX, and Gateway Load.
- ETX is calculated using sequence-gap detection (zero overhead).
