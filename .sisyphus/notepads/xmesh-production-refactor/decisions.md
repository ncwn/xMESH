# Decisions - xMESH Production Refactor

This file tracks architectural choices and design decisions made during execution.

---
## Research Snapshot Created

- Created local branch `research-ref` from `origin/research`.
- Exported 10 key research files to `.sisyphus/research-snapshot/` to preserve algorithms and configurations before resetting the main branch.
- Verified `main.cpp` contains 2116 lines of code covering TrickleTimer, CostRouter, ETXTracker, and GatewayBalancer logic.

## Main Branch Reset to Upstream LoRaMesher

- Reset the `main` branch to match `upstream/main` (commit `d37dee7`).
- Created backup tag `backup-before-upstream-reset` to preserve the previous state of `main`.
- This provides a clean v0.0.11 LoRaMesher base for the production refactor, as research modifications are now preserved in `.sisyphus/research-snapshot/` and the `research-ref` branch.
- Verified that `library.json` now correctly identifies the library as "LoRaMesher" and that core source files are in place.

## Callback Hooks Added to LoRaMesher

- Created `feature/xmesh-callbacks` branch from main
- Added two callback typedefs to `RoutingTableService.h`:
  - `CostCalculationCallback`: Enables custom cost calculation for route selection
  - `HelloReceivedCallback`: Enables HELLO packet interception for Trickle scheduler
- Callbacks are **optional** (nullptr by default) - preserves 100% LoRaMesher compatibility
- Cost callback includes 15% hysteresis threshold to prevent route flapping
- Added `LM_GOD_MODE` build flag to `BuildOptions.h` for future xMESH features
- Commit: 7a10a11 "feat(routing): add cost and HELLO callback hooks"

**Design Decisions**:
1. Used function pointer typedefs (not std::function) for embedded efficiency
2. Static callbacks (not instance-based) - LoRaMesher uses static service pattern
3. Cost comparison with 0.85x hysteresis prevents oscillation between similar-cost routes
4. HELLO callback invoked AFTER processRoute() completes (ensures routing table updated first)
5. Callbacks check for nullptr before invocation (zero overhead when disabled)

**Implementation Notes**:
- Callback invocations are placed in existing LoRaMesher code paths
- Cost-based routing replaces hop-count comparison when `costCallback != nullptr`
- HELLO callback fires on every route packet reception (includes sender in HELLO broadcast)
- Setter methods log when callbacks are enabled for debugging visibility
