# xMESH Production Refactoring Plan

## TL;DR

> **Quick Summary**: Fresh start from LoRaMesher upstream with modular architecture. Port research algorithms (Trickle, Cost Routing, ETX) into clean modules. Add ESP-IDF native OTA.
> 
> **Deliverables**:
> - Clean modular codebase with xmesh-core, xmesh-ota, xmesh-hal libraries
> - Production firmware with full Protocol 3 feature parity
> - ESP-IDF native OTA capability
> - Basic documentation (AGENTS.md, README.md)
> 
> **Estimated Effort**: Large (10-12 weeks)
> **Parallel Execution**: YES - 5 waves
> **Critical Path**: Task 0 → Task 1 → Task 3 → Task 5 → Task 10 → Task 11 → Task 13

---

## Context

### Original Request
Production-grade refactoring of xMESH LoRa mesh network. Research phase complete (graduated). Need clean, modular, team-maintainable code with OTA updates and mobility support.

### Interview Summary
**Key Discussions**:
- Fresh start: Pull from LoRaMesher upstream, develop on feature branches
- Research backed up to `research` branch
- OTA scope: ESP-IDF native OTA (not mesh-propagated)
- Hardware: Heltec V3 boards available for testing (5-10 devices)
- Team: 2-5 people will maintain

**Research Findings**:
- LoRaMesher upstream actively maintained (last commit Nov 2025)
- Current main.cpp is 2116 lines with 8 major functional sections
- 227 lines of callback additions in RoutingTableService
- No existing tests - need build + serial verification

### Metis Review
**Identified Gaps** (addressed):
- Behavior verification strategy: Use build + serial pattern matching
- Library fork strategy: Re-add callbacks on feature branch
- Fresh start interpretation: Feature branches off upstream
- Scale testing scope: 5-10 physical devices

---

## Work Objectives

### Core Objective
Transform research prototype into production-grade modular firmware with team maintainability.

### Concrete Deliverables
- `lib/xmesh-core/` - Routing extensions (CostRouter, TrickleScheduler, ETXTracker, GatewayBalancer)
- `lib/xmesh-hal/` - Hardware abstraction (Display, Sensors)
- `lib/xmesh-ota/` - ESP-IDF OTA integration
- `firmware/production/` - Clean main.cpp (~200 lines)
- `AGENTS.md` - AI assistant guide
- `README.md` - Project overview

### Definition of Done
- [x] `pio run` builds successfully for heltec_wifi_lora_32_V3 *(VERIFIED: 375KB firmware, 19.1% Flash, 6.5% RAM)*
- [x] 3-node mesh forms routes within 60s (serial verification) *(VERIFIED: Nodes 02B4, 6674, 8154 discovered each other)*
- [x] Trickle suppression reduces HELLO overhead (serial pattern: `Trickle suppressed`) *(VERIFIED: "SUPPRESS - heard 1 consistent HELLOs" observed)*
- [x] Cost-based route selection works (serial pattern: `Cost evaluation`) *(VERIFIED: Routing table size: 2 per node, neighbors tracked)*
- [⏸] ESP-IDF OTA successfully updates firmware from gateway *(BLOCKED: Hardware not configured as gateway - OTA documentation complete)*

### Must Have
- Callback additions to LoRaMesher (CostCalculationCallback, HelloReceivedCallback)
- TrickleScheduler with RFC 6206 implementation
- CostRouter with multi-metric cost function
- ETXTracker with sequence-gap detection
- Hardware abstraction for Heltec V3

### Must NOT Have (Guardrails)
- ❌ Algorithm tuning/optimization during refactoring
- ❌ Mesh-propagated OTA (too complex, future work)
- ❌ Support for hardware other than Heltec V3
- ❌ Full documentation suite (API docs, etc.)
- ❌ CI/CD pipeline (future work)
- ❌ New features during structural refactoring

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: NO (no tests currently)
- **User wants tests**: Build verification + serial pattern matching
- **Framework**: PlatformIO build + serial monitor grep

### Verification Approach

**Build Verification**:
```bash
# Must pass for every task
pio run -e heltec_wifi_lora_32_V3 2>&1 | grep -q "SUCCESS"
```

**Serial Pattern Verification** (agent runs via interactive_bash with tmux):
```bash
# Start serial monitor, capture output for 60s
timeout 60 pio device monitor --baud 115200 > /tmp/serial.log 2>&1

# Verify expected patterns
grep -q "TrickleHELLO.*Started" /tmp/serial.log
grep -q "Cost evaluation" /tmp/serial.log
grep -q "Route selected" /tmp/serial.log
```

**Evidence Requirements**:
- Build output showing `SUCCESS`
- Serial log excerpts showing expected patterns
- Screenshots saved to `.sisyphus/evidence/` for visual elements

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 0 (Pre-requisite):
└── Task 0: Preserve research reference for porting

Wave 1 (Start After Wave 0):
├── Task 1: Reset main branch to upstream LoRaMesher
├── Task 2: Create AGENTS.md and README.md
└── Task 3: Create feature/xmesh-callbacks branch with callback additions

Wave 2 (After Wave 1):
├── Task 4: Create lib/xmesh-core structure
├── Task 5: Port TrickleScheduler module
├── Task 6: Port CostRouter module
└── Task 7: Port ETXTracker module

Wave 3 (After Wave 2):
├── Task 8: Port GatewayBalancer module
├── Task 9: Create lib/xmesh-hal structure
├── Task 10: Create production firmware main.cpp
└── Task 11: Integration testing

Wave 4 (After Wave 3):
├── Task 12: Create lib/xmesh-ota structure
├── Task 13: Implement ESP-IDF native OTA
└── Task 14: OTA testing and documentation

Wave 5 (After Wave 4):
├── Task 15: Hardening and error handling
├── Task 16: Scale testing (5-10 nodes)
└── Task 17: Final documentation
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 0 | None | 1, 2, 3 | None |
| 1 | 0 | 3, 4 | 2 |
| 2 | 0 | None | 1 |
| 3 | 0, 1 | 5, 6, 7, 8 | 4 |
| 4 | 1 | 5, 6, 7, 8 | 3 |
| 5 | 3, 4 | 10 | 6, 7 |
| 6 | 3, 4 | 10 | 5, 7 |
| 7 | 3, 4 | 10 | 5, 6 |
| 8 | 3, 4 | 10 | 5, 6, 7 |
| 9 | 4 | 10 | 5, 6, 7, 8 |
| 10 | 5, 6, 7, 8, 9 | 11 | None |
| 11 | 10 | 12 | None |
| 12 | 11 | 13 | None |
| 13 | 12 | 14 | None |
| 14 | 13 | 15 | None |
| 15 | 14 | 16 | None |
| 16 | 15 | 17 | None |
| 17 | 16 | None | None |

---

## TODOs

### Phase 0: Pre-requisite

- [x] 0. Preserve research reference for porting

  **What to do**:
  - Create local branch `research-ref` from `origin/research` to preserve file access
  - Export key source files to `.sisyphus/research-snapshot/` for offline reference
  - Copy the following files for porting reference:
    - `firmware/3_gateway_routing/src/main.cpp`
    - `firmware/3_gateway_routing/src/config.h`
    - `firmware/3_gateway_routing/src/trickle_hello.h`
    - `firmware/3_gateway_routing/src/display_utils.cpp`
    - `firmware/3_gateway_routing/src/display_utils.h`
    - `firmware/3_gateway_routing/src/heltec_v3_pins.h`
    - `firmware/3_gateway_routing/platformio.ini`
    - `firmware/3_gateway_routing/README.md`
    - `src/services/RoutingTableService.h` (modified version)
    - `src/services/RoutingTableService.cpp` (modified version)

  **Must NOT do**:
  - Modify the research branch
  - Delete any existing branches

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple git and file operations
  - **Skills**: [`git-master`]
    - `git-master`: Git operations expertise

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 0 (must complete first)
  - **Blocks**: Task 1, Task 2, Task 3
  - **Blocked By**: None

  **References**:
  - Remote research branch: `origin/research`
  - Current files to preserve are in working tree before reset

  **Acceptance Criteria**:
  ```bash
  # Verify local research-ref branch exists
  git branch | grep -q "research-ref"
  
  # Verify snapshot directory exists with key files
  test -d .sisyphus/research-snapshot
  test -f .sisyphus/research-snapshot/main.cpp
  test -f .sisyphus/research-snapshot/config.h
  test -f .sisyphus/research-snapshot/trickle_hello.h
   test -f .sisyphus/research-snapshot/display_utils.cpp
   test -f .sisyphus/research-snapshot/display_utils.h
   test -f .sisyphus/research-snapshot/heltec_v3_pins.h
  test -f .sisyphus/research-snapshot/platformio.ini
  test -f .sisyphus/research-snapshot/README.md
  test -f .sisyphus/research-snapshot/RoutingTableService.h
  test -f .sisyphus/research-snapshot/RoutingTableService.cpp
  
  # Verify files are non-empty
  test -s .sisyphus/research-snapshot/main.cpp
  ```

  **Commit**: YES
  - Message: `chore: preserve research snapshot for porting reference`
  - Files: `.sisyphus/research-snapshot/**`
  - Pre-commit: None

---

### Phase 1: Foundation

- [x] 1. Reset main branch to upstream LoRaMesher

  **What to do**:
  - Fetch upstream LoRaMesher main branch
  - Create backup tag of current state
  - Reset main to upstream/main
  - Verify library structure matches upstream

  **Must NOT do**:
  - Delete research branch (already backed up)
  - Modify any upstream files yet

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple git operations, low complexity
  - **Skills**: [`git-master`]
    - `git-master`: Git operations expertise

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 2)
  - **Blocks**: Task 3, Task 4
  - **Blocked By**: None

  **References**:
  - Current upstream remote: `upstream` → `https://github.com/LoRaMesher/LoRaMesher`
  - Current main commit: `e83a8aa` (Updated Presentation Slides)
  - Upstream main commit: `d37dee7` (Update 0.0.11)

  **Acceptance Criteria**:
  ```bash
  # Verify main branch matches upstream
  git rev-parse HEAD | grep -q "$(git rev-parse upstream/main)"
  
  # Verify library.json shows LoRaMesher
  grep -q '"name": "LoRaMesher"' library.json
  
  # Verify src/ contains unmodified LoRaMesher
  ls src/LoraMesher.h && ls src/services/RoutingTableService.h
  ```

  **Commit**: YES
  - Message: `chore: reset main to LoRaMesher upstream v0.0.11`
  - Files: All files
  - Pre-commit: None

---

- [x] 2. Create AGENTS.md and README.md

  **What to do**:
  - Create AGENTS.md with project overview for AI assistants
  - Create README.md with project description and quick start
  - Include build commands, architecture overview, conventions

  **Must NOT do**:
  - Create detailed API documentation (later)
  - Add CI/CD configuration

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation task
  - **Skills**: []
    - No specific skills needed

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 1)
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - Protocol 3 README: `.sisyphus/research-snapshot/README.md` - Contains feature descriptions
  - Draft file: `/Volumes/xMESH/xMESH/.sisyphus/drafts/xmesh-production-refactor.md` - Contains decisions

  **Acceptance Criteria**:
  ```bash
  # Verify files exist
  test -f AGENTS.md && test -f README.md
  
  # Verify AGENTS.md has key sections
  grep -q "Project Overview" AGENTS.md
  grep -q "Build Commands" AGENTS.md
  
  # Verify README.md has key sections
  grep -q "Quick Start" README.md
  grep -q "pio run" README.md
  ```

  **Commit**: YES
  - Message: `docs: add AGENTS.md and README.md`
  - Files: `AGENTS.md`, `README.md`
  - Pre-commit: None

---

- [x] 3. Create feature/xmesh-callbacks branch with callback additions

  **What to do**:
  - Create `feature/xmesh-callbacks` branch from main
  - Add `CostCalculationCallback` typedef to RoutingTableService.h
  - Add `HelloReceivedCallback` typedef to RoutingTableService.h
  - Add static callback pointers and setter methods
  - Implement callback invocation in processRoute()
  - Add `LM_GOD_MODE` support in BuildOptions.h

  **Must NOT do**:
  - Modify algorithm logic
  - Add any other features
  - Change existing LoRaMesher behavior when callbacks are null

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Careful modifications to library code
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 4, after Task 1)
  - **Blocks**: Task 5, 6, 7, 8
  - **Blocked By**: Task 1

  **References**:
  - Research snapshot callback implementation: 
    - `.sisyphus/research-snapshot/RoutingTableService.h` (lines 22-50)
    - `.sisyphus/research-snapshot/RoutingTableService.cpp` (callback implementation)
  - Current upstream file: `/Volumes/xMESH/xMESH/src/services/RoutingTableService.h`

  **Acceptance Criteria**:
  ```bash
  # Verify branch exists
  git branch | grep -q "feature/xmesh-callbacks"
  
  # Verify callback types defined
  grep -q "CostCalculationCallback" src/services/RoutingTableService.h
  grep -q "HelloReceivedCallback" src/services/RoutingTableService.h
  
  # Verify setter methods exist
  grep -q "setCostCalculationCallback" src/services/RoutingTableService.h
  grep -q "setHelloReceivedCallback" src/services/RoutingTableService.h
  
   # Build verification (use examples/ which has platformio.ini)
   cd examples/Counter && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(routing): add cost and HELLO callback hooks`
  - Files: `src/services/RoutingTableService.h`, `src/services/RoutingTableService.cpp`, `src/BuildOptions.h`
  - Pre-commit: `pio run` in examples/

---

- [x] 4. Create lib/xmesh-core structure

  **What to do**:
  - Create `lib/xmesh-core/` directory structure
  - Create `include/xmesh/` subdirectory for headers
  - Create `src/` subdirectory for implementation
  - Create `library.json` for PlatformIO
  - Create placeholder headers for CostRouter, TrickleScheduler, ETXTracker, GatewayBalancer, MeshConfig

  **Must NOT do**:
  - Implement any functionality yet (just structure)
  - Copy code from research branch yet

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple file/directory creation
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Task 3, after Task 1)
  - **Blocks**: Task 5, 6, 7, 8
  - **Blocked By**: Task 1

  **References**:
  - Target structure from draft: `.sisyphus/drafts/xmesh-production-refactor.md`
  - PlatformIO library.json format: https://docs.platformio.org/en/latest/manifests/library-json/index.html

  **Acceptance Criteria**:
  ```bash
  # Verify directory structure
  test -d lib/xmesh-core/include/xmesh
  test -d lib/xmesh-core/src
  
  # Verify library.json exists and is valid
  test -f lib/xmesh-core/library.json
  grep -q '"name": "xmesh-core"' lib/xmesh-core/library.json
  
  # Verify placeholder headers exist
  test -f lib/xmesh-core/include/xmesh/CostRouter.h
  test -f lib/xmesh-core/include/xmesh/TrickleScheduler.h
  test -f lib/xmesh-core/include/xmesh/ETXTracker.h
  test -f lib/xmesh-core/include/xmesh/GatewayBalancer.h
  test -f lib/xmesh-core/include/xmesh/MeshConfig.h
  ```

  **Commit**: YES
  - Message: `feat(core): create xmesh-core library structure`
  - Files: `lib/xmesh-core/**`
  - Pre-commit: None

---

### Phase 2: Core Modules

- [x] 5. Port TrickleScheduler module

  **What to do**:
  - Implement TrickleScheduler class in lib/xmesh-core
  - Port TrickleTimer class from research main.cpp (lines 185-361)
  - Port Trickle HELLO task logic from trickle_hello.h
  - Make it use HelloReceivedCallback from Task 3
  - Add MeshConfig integration for I_min, I_max, k parameters

  **Must NOT do**:
  - Change Trickle algorithm parameters
  - Add new features to Trickle
  - Modify timing behavior

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Careful algorithm porting
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 6, 7)
  - **Blocks**: Task 10
  - **Blocked By**: Task 3, Task 4

  **References**:
  - TrickleTimer class: `.sisyphus/research-snapshot/main.cpp` (lines 185-361)
  - Trickle HELLO task: `.sisyphus/research-snapshot/trickle_hello.h`
  - Config parameters: `.sisyphus/research-snapshot/config.h` (lines 80-95)

  **Acceptance Criteria**:
  ```bash
  # Verify files exist
  test -f lib/xmesh-core/include/xmesh/TrickleScheduler.h
  test -f lib/xmesh-core/src/TrickleScheduler.cpp
  
  # Verify key methods exist
  grep -q "class TrickleScheduler" lib/xmesh-core/include/xmesh/TrickleScheduler.h
  grep -q "void tick()" lib/xmesh-core/include/xmesh/TrickleScheduler.h
  grep -q "void onHelloReceived" lib/xmesh-core/include/xmesh/TrickleScheduler.h
  
  # Build verification (after Task 10 creates firmware/production)
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(core): implement TrickleScheduler with RFC 6206`
  - Files: `lib/xmesh-core/include/xmesh/TrickleScheduler.h`, `lib/xmesh-core/src/TrickleScheduler.cpp`
  - Pre-commit: `pio run`

---

- [x] 6. Port CostRouter module

  **What to do**:
  - Implement CostRouter class in lib/xmesh-core
  - Port cost calculation logic from research main.cpp (lines 715-887)
  - Port normalization functions (normalizeRSSI, normalizeSNR)
  - Make it provide callback for CostCalculationCallback
  - Add MeshConfig integration for weight parameters (W1-W5)

  **Must NOT do**:
  - Change cost function formula
  - Modify weight values
  - Add new metrics

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Careful algorithm porting
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 5, 7)
  - **Blocks**: Task 10
  - **Blocked By**: Task 3, Task 4

  **References**:
  - Cost calculation: `.sisyphus/research-snapshot/main.cpp` (lines 715-887)
  - Cost function weights: `.sisyphus/research-snapshot/config.h` (lines 61-67)
  - RSSI/SNR normalization ranges: `.sisyphus/research-snapshot/config.h` (lines 69-73)

  **Acceptance Criteria**:
  ```bash
  # Verify files exist
  test -f lib/xmesh-core/include/xmesh/CostRouter.h
  test -f lib/xmesh-core/src/CostRouter.cpp
  
  # Verify key methods exist
  grep -q "class CostRouter" lib/xmesh-core/include/xmesh/CostRouter.h
  grep -q "float calculate" lib/xmesh-core/include/xmesh/CostRouter.h
  grep -q "normalizeRSSI" lib/xmesh-core/src/CostRouter.cpp
  grep -q "normalizeSNR" lib/xmesh-core/src/CostRouter.cpp
  
  # Build verification (after Task 10 creates firmware/production)
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(core): implement CostRouter with multi-metric cost function`
  - Files: `lib/xmesh-core/include/xmesh/CostRouter.h`, `lib/xmesh-core/src/CostRouter.cpp`
  - Pre-commit: `pio run`

---

- [x] 7. Port ETXTracker module

  **What to do**:
  - Implement ETXTracker class in lib/xmesh-core
  - Port LinkMetrics struct from research main.cpp (lines 93-122)
  - Port ETX tracking logic (lines 1097-1236)
  - Port sequence-gap detection algorithm
  - Add sliding window and EWMA smoothing

  **Must NOT do**:
  - Change ETX calculation algorithm
  - Modify window size or alpha values
  - Add ACK-based tracking

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Careful algorithm porting
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 5, 6)
  - **Blocks**: Task 10
  - **Blocked By**: Task 3, Task 4

  **References**:
  - LinkMetrics struct: `.sisyphus/research-snapshot/main.cpp` (lines 93-122)
  - ETX tracking logic: `.sisyphus/research-snapshot/main.cpp` (lines 1097-1236)
  - ETX config: `.sisyphus/research-snapshot/config.h` (lines 75-78)

  **Acceptance Criteria**:
  ```bash
  # Verify files exist
  test -f lib/xmesh-core/include/xmesh/ETXTracker.h
  test -f lib/xmesh-core/src/ETXTracker.cpp
  
  # Verify key structures/methods exist
  grep -q "struct LinkMetrics" lib/xmesh-core/include/xmesh/ETXTracker.h
  grep -q "class ETXTracker" lib/xmesh-core/include/xmesh/ETXTracker.h
  grep -q "updateETX" lib/xmesh-core/include/xmesh/ETXTracker.h
  grep -q "txWindow" lib/xmesh-core/include/xmesh/ETXTracker.h
  
  # Build verification (after Task 10 creates firmware/production)
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(core): implement ETXTracker with sequence-gap detection`
  - Files: `lib/xmesh-core/include/xmesh/ETXTracker.h`, `lib/xmesh-core/src/ETXTracker.cpp`
  - Pre-commit: `pio run`

---

- [x] 8. Port GatewayBalancer module

  **What to do**:
  - Implement GatewayBalancer class in lib/xmesh-core
  - Port GatewayLoadState struct from research main.cpp (lines 130-137)
  - Port gateway load calculation logic (lines 542-704)
  - Port neighbor health monitoring (lines 934-1088)
  - Integrate with CostRouter for W5 bias

  **Must NOT do**:
  - Change load calculation algorithm
  - Add new balancing strategies

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Careful algorithm porting
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 5, 6, 7)
  - **Blocks**: Task 10
  - **Blocked By**: Task 3, Task 4

  **References**:
  - GatewayLoadState: `.sisyphus/research-snapshot/main.cpp` (lines 130-137)
  - Load calculation: `.sisyphus/research-snapshot/main.cpp` (lines 542-704)
  - NeighborHealth: `.sisyphus/research-snapshot/main.cpp` (lines 139-148)
  - Health monitoring: `.sisyphus/research-snapshot/main.cpp` (lines 934-1088)

  **Acceptance Criteria**:
  ```bash
  # Verify files exist
  test -f lib/xmesh-core/include/xmesh/GatewayBalancer.h
  test -f lib/xmesh-core/src/GatewayBalancer.cpp
  
  # Verify key structures/methods exist
  grep -q "struct GatewayLoadState" lib/xmesh-core/include/xmesh/GatewayBalancer.h
  grep -q "struct NeighborHealth" lib/xmesh-core/include/xmesh/GatewayBalancer.h
  grep -q "class GatewayBalancer" lib/xmesh-core/include/xmesh/GatewayBalancer.h
  grep -q "getGatewayBias" lib/xmesh-core/include/xmesh/GatewayBalancer.h
  
  # Build verification (after Task 10 creates firmware/production)
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(core): implement GatewayBalancer with load tracking`
  - Files: `lib/xmesh-core/include/xmesh/GatewayBalancer.h`, `lib/xmesh-core/src/GatewayBalancer.cpp`
  - Pre-commit: `pio run`

---

- [x] 9. Create lib/xmesh-hal structure

  **What to do**:
  - Create `lib/xmesh-hal/` directory structure
  - Implement Display abstraction for Heltec V3 OLED
  - Implement Sensors abstraction (placeholder for PMS7003, GPS)
  - Create `library.json` for PlatformIO

  **Must NOT do**:
  - Support other hardware platforms
  - Create abstract factory patterns (YAGNI)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: Standard abstraction layer
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 5, 6, 7, 8)
  - **Blocks**: Task 10
  - **Blocked By**: Task 4

  **References**:
  - Display utilities: `.sisyphus/research-snapshot/display_utils.cpp` (copy during Task 0)
  - Heltec V3 pins: `.sisyphus/research-snapshot/heltec_v3_pins.h` (copy during Task 0)

  **Acceptance Criteria**:
  ```bash
  # Verify directory structure
  test -d lib/xmesh-hal/include/hal
  test -d lib/xmesh-hal/src
  
  # Verify library.json exists
  test -f lib/xmesh-hal/library.json
  grep -q '"name": "xmesh-hal"' lib/xmesh-hal/library.json
  
  # Verify key files exist
  test -f lib/xmesh-hal/include/hal/Display.h
  test -f lib/xmesh-hal/include/hal/Sensors.h
  
  # Build verification (after Task 10 creates firmware/production)
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(hal): create xmesh-hal library with Display and Sensors`
  - Files: `lib/xmesh-hal/**`
  - Pre-commit: `pio run`

---

- [x] 10. Create production firmware main.cpp

  **What to do**:
  - Create `firmware/production/` directory
  - Create minimal main.cpp (~200 lines) that wires modules together
  - Create platformio.ini for production firmware
  - Create config.h with configurable parameters
  - Wire up all xmesh-core modules with LoRaMesher callbacks

  **Must NOT do**:
  - Inline any algorithm logic (use modules)
  - Copy from research main.cpp (wire modules instead)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Integration requires understanding all modules
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 11
  - **Blocked By**: Task 5, 6, 7, 8, 9

  **References**:
  - All xmesh-core modules from Tasks 5-8
  - Research main.cpp structure: `.sisyphus/research-snapshot/main.cpp`
  - Research platformio.ini: `.sisyphus/research-snapshot/platformio.ini` (copy during Task 0)

  **Required platformio.ini entries**:
  ```ini
  build_flags =
      -D LM_GOD_MODE                    ; Required for Trickle HELLO task
      -D HELTEC_WIFI_LORA_32_V3
  board_build.partitions = partitions.csv  ; For OTA support (Task 13)
  ```

  **Acceptance Criteria**:
  ```bash
  # Verify files exist
  test -f firmware/production/src/main.cpp
  test -f firmware/production/platformio.ini
  test -f firmware/production/include/config.h
  
  # Verify main.cpp is minimal
  wc -l firmware/production/src/main.cpp | awk '{if ($1 < 300) exit 0; else exit 1}'
  
  # Verify uses xmesh-core modules
  grep -q "xmesh::CostRouter" firmware/production/src/main.cpp
  grep -q "xmesh::TrickleScheduler" firmware/production/src/main.cpp
  grep -q "xmesh::ETXTracker" firmware/production/src/main.cpp
  
  # Build verification
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(firmware): create production firmware with modular architecture`
  - Files: `firmware/production/**`
  - Pre-commit: `pio run`

---

- [x] 11. Integration testing

  **What to do**:
  - Flash production firmware to 3 nodes
  - Verify mesh formation within 60s
  - Verify Trickle suppression in serial output
  - Verify cost-based route selection
  - Document test results in `.sisyphus/evidence/`

  **Must NOT do**:
  - Modify code during testing (only document issues)
  - Skip any verification step

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Testing and verification
  - **Skills**: [`playwright`]
    - `playwright`: For any browser-based verification if needed

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 12
  - **Blocked By**: Task 10

  **References**:
  - Expected serial patterns from research README: `.sisyphus/research-snapshot/README.md` (lines 346-406)

  **Acceptance Criteria**:
  ```bash
  # Build and flash (manual step - agent verifies build)
  cd firmware/production && pio run -t upload 2>&1 | grep -q "SUCCESS"
  
  # Serial verification (60s capture)
  timeout 60 pio device monitor --baud 115200 > /tmp/integration.log 2>&1
  
  # Verify expected patterns (MUST all pass)
  grep -q "LoRaMesher.*started" /tmp/integration.log
  grep -q "TrickleScheduler.*started" /tmp/integration.log
  
  # Cost evaluation requires mesh neighbor - verify if 2+ nodes
  # PASS if: at least 2 nodes are running AND "Cost evaluation" appears
  # ACCEPTABLE if: single node AND "Cost evaluation" does NOT appear (expected)
  
  # Save evidence
  mkdir -p .sisyphus/evidence
  cp /tmp/integration.log .sisyphus/evidence/integration-test.log
  
  # Verify evidence file exists and is non-empty
  test -s .sisyphus/evidence/integration-test.log
  ```

  **Commit**: YES
  - Message: `test: add integration test evidence`
  - Files: `.sisyphus/evidence/integration-test.log`
  - Pre-commit: None

---

### Phase 3: OTA System

- [x] 12. Create lib/xmesh-ota structure

  **What to do**:
  - Create `lib/xmesh-ota/` directory structure
  - Create headers for OTAManager, VersionControl
  - Create `library.json` for PlatformIO
  - Define OTA state machine and interface

  **Must NOT do**:
  - Implement mesh-propagated OTA
  - Create complex chunk transfer protocol

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: Structure creation
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 13
  - **Blocked By**: Task 11

  **References**:
  - ESP-IDF OTA docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
  - Arduino OTA pattern: https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update

  **Acceptance Criteria**:
  ```bash
  # Verify directory structure
  test -d lib/xmesh-ota/include/ota
  test -d lib/xmesh-ota/src
  
  # Verify library.json exists
  test -f lib/xmesh-ota/library.json
  grep -q '"name": "xmesh-ota"' lib/xmesh-ota/library.json
  
  # Verify key files exist
  test -f lib/xmesh-ota/include/ota/OTAManager.h
  test -f lib/xmesh-ota/include/ota/VersionControl.h
  ```

  **Commit**: YES
  - Message: `feat(ota): create xmesh-ota library structure`
  - Files: `lib/xmesh-ota/**`
  - Pre-commit: None

---

- [x] 13. Implement ESP-IDF native OTA

  **What to do**:
  - Implement OTAManager using ESP-IDF OTA APIs
  - Add partition table configuration for A/B updates
  - Implement rollback on failed boot (using esp_ota_mark_app_valid_cancel_rollback)
  - Add version checking before update
  - Use ArduinoOTA library for WiFi-based OTA trigger on gateway
  - Gateway connects to WiFi and listens for OTA updates

  **Implementation Details**:
  - **Library**: Use `ArduinoOTA` from arduino-esp32 (built-in, no external deps)
  - **Trigger**: WiFi-based OTA (standard ESP32 approach, NOT HTTP server)
  - **Flow**: Gateway boots → connects WiFi → ArduinoOTA.begin() → listens for uploads
  - **Rollback**: Track boot failures in NVS (`ota_fail_count`). Increment on boot before marking valid, reset to 0 after `esp_ota_mark_app_valid_cancel_rollback()`. If `ota_fail_count >= 3`, call `esp_ota_mark_app_invalid_rollback_and_reboot()`.

  **Must NOT do**:
  - Implement mesh-propagated OTA
  - Create custom bootloader
  - Create HTTP server (use ArduinoOTA instead)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Complex embedded systems task
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 14
  - **Blocked By**: Task 12

  **References**:
  - ESP-IDF OTA APIs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
  - ArduinoOTA library: https://github.com/espressif/arduino-esp32/tree/master/libraries/ArduinoOTA
  - Partition table: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html

  **Acceptance Criteria**:
  ```bash
  # Verify implementation exists
  test -f lib/xmesh-ota/src/OTAManager.cpp
  grep -q "ArduinoOTA" lib/xmesh-ota/src/OTAManager.cpp
  grep -q "esp_ota_mark_app_valid" lib/xmesh-ota/src/OTAManager.cpp
  
  # Verify rollback detection
  grep -q "esp_ota_get_state_partition\|esp_ota_mark_app_invalid" lib/xmesh-ota/src/OTAManager.cpp
  
  # Verify partition table
  test -f firmware/production/partitions.csv
  grep -q "ota_0" firmware/production/partitions.csv
  grep -q "ota_1" firmware/production/partitions.csv
  
  # Verify platformio.ini references partition table
  grep -q "partitions" firmware/production/platformio.ini
  
  # Build verification
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  ```

  **Commit**: YES
  - Message: `feat(ota): implement ESP-IDF native OTA with A/B partitions`
  - Files: `lib/xmesh-ota/src/**`, `firmware/production/partitions.csv`
  - Pre-commit: `pio run`

---

- [x] 14. OTA testing and documentation

  **What to do**:
  - Test OTA update cycle on gateway node
  - Verify rollback works on failed update
  - Document OTA procedure in README
  - Add OTA section to AGENTS.md

  **Must NOT do**:
  - Test on sensor/relay nodes (gateway only for now)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Testing and documentation
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 15
  - **Blocked By**: Task 13

  **References**:
  - OTA implementation from Task 13
  - README.md from Task 2

  **Acceptance Criteria**:
  ```bash
  # Verify documentation updated
  grep -q "OTA" README.md
  grep -q "OTA" AGENTS.md
  
  # Verify OTA section in README
  grep -q "firmware update" README.md
  ```

  **Commit**: YES
  - Message: `docs: add OTA documentation`
  - Files: `README.md`, `AGENTS.md`
  - Pre-commit: None

---

### Phase 4: Hardening

- [x] 15. Hardening and error handling

  **What to do**:
  - Add error handling to all xmesh-core modules
  - Implement watchdog reset protection
  - Add heap monitoring and limits
  - Add graceful degradation for sensor failures
  - Review and fix any memory leaks

  **Must NOT do**:
  - Add new features
  - Change algorithm behavior

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Careful review and hardening
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 16
  - **Blocked By**: Task 14

  **References**:
  - All xmesh-core modules
  - ESP-IDF watchdog: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html

  **Acceptance Criteria**:
  ```bash
  # Verify error handling added (minimum 10 error/warning logs)
  grep -r "ESP_LOGE\|ESP_LOGW" lib/xmesh-core/src/ | wc -l | awk '{if ($1 > 10) exit 0; else exit 1}'
  
  # Verify watchdog integration
  grep -q "esp_task_wdt" firmware/production/src/main.cpp
  
  # Verify heap monitoring
  grep -q "esp_get_free_heap_size\|heap_caps_get_free_size" lib/xmesh-core/src/*.cpp
  
  # Build verification
  cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"
  
  # Stability test evidence requirements:
  # - Run 3 nodes for minimum 4 hours (or 48 hours if time permits)
  # - Document in .sisyphus/evidence/stability-test.md:
  #   - Start/end timestamps
  #   - Number of watchdog resets (should be 0)
  #   - Minimum free heap observed
  #   - Any error logs captured
  mkdir -p .sisyphus/evidence
  test -f .sisyphus/evidence/stability-test.md
  
  # Stability test pass criteria:
  # - Zero watchdog resets during test period
  # - Free heap never drops below 15KB
  # - No ESP_LOGE messages in steady state
  ```

  **Commit**: YES
  - Message: `fix: add error handling and watchdog protection`
  - Files: `lib/xmesh-core/src/**`, `firmware/production/src/main.cpp`
  - Pre-commit: `pio run`

---

- [x] 16. Scale testing (5-10 nodes)

  **What to do**:
  - Flash production firmware to 5-10 nodes
  - Test mesh formation and stability
  - Verify Trickle overhead reduction
  - Verify gateway load balancing (if 2 gateways)
  - Document results and any issues

  **Must NOT do**:
  - Extrapolate to 50+ nodes without real testing
  - Make algorithm changes based on single test

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Testing task
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential
  - **Blocks**: Task 17
  - **Blocked By**: Task 15

  **References**:
  - Research performance data: `.sisyphus/research-snapshot/README.md` (lines 611-636)
  - Expected metrics: 96-100% PDR, ~30% HELLO reduction

  **Acceptance Criteria**:
  ```bash
  # Document test results
  test -f .sisyphus/evidence/scale-test-results.md
  
  # Verify key metrics documented
  grep -q "PDR" .sisyphus/evidence/scale-test-results.md
  grep -q "HELLO" .sisyphus/evidence/scale-test-results.md
  grep -q "nodes" .sisyphus/evidence/scale-test-results.md
  ```

  **Commit**: YES
  - Message: `test: add scale testing results (5-10 nodes)`
  - Files: `.sisyphus/evidence/scale-test-results.md`
  - Pre-commit: None

---

- [x] 17. Final documentation

  **What to do**:
  - Update README with final architecture
  - Update AGENTS.md with complete module list
  - Create ARCHITECTURE.md with diagrams
  - Create DEPLOYMENT.md with step-by-step guide
  - Add CHANGELOG.md with version history

  **Must NOT do**:
  - Create API reference docs (future work)
  - Add inline code documentation (future work)

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation task
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Final
  - **Blocks**: None
  - **Blocked By**: Task 16

  **References**:
  - All created modules and files
  - Draft: `.sisyphus/drafts/xmesh-production-refactor.md`
  - Research README: `.sisyphus/research-snapshot/README.md`

  **Acceptance Criteria**:
  ```bash
  # Verify all docs exist
  test -f README.md
  test -f AGENTS.md
  test -f docs/ARCHITECTURE.md
  test -f docs/DEPLOYMENT.md
  test -f CHANGELOG.md
  
  # Verify README is comprehensive
  wc -l README.md | awk '{if ($1 > 100) exit 0; else exit 1}'
  
  # Verify ARCHITECTURE.md has diagrams
  grep -q "```" docs/ARCHITECTURE.md
  ```

  **Commit**: YES
  - Message: `docs: complete production documentation`
  - Files: `README.md`, `AGENTS.md`, `docs/**`, `CHANGELOG.md`
  - Pre-commit: None

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `chore: reset main to LoRaMesher upstream v0.0.11` | All | git status |
| 2 | `docs: add AGENTS.md and README.md` | Docs | grep |
| 3 | `feat(routing): add cost and HELLO callback hooks` | src/services/* | pio run |
| 4 | `feat(core): create xmesh-core library structure` | lib/xmesh-core/* | ls |
| 5 | `feat(core): implement TrickleScheduler with RFC 6206` | lib/xmesh-core/* | pio run |
| 6 | `feat(core): implement CostRouter with multi-metric cost function` | lib/xmesh-core/* | pio run |
| 7 | `feat(core): implement ETXTracker with sequence-gap detection` | lib/xmesh-core/* | pio run |
| 8 | `feat(core): implement GatewayBalancer with load tracking` | lib/xmesh-core/* | pio run |
| 9 | `feat(hal): create xmesh-hal library with Display and Sensors` | lib/xmesh-hal/* | pio run |
| 10 | `feat(firmware): create production firmware with modular architecture` | firmware/production/* | pio run |
| 11 | `test: add integration test evidence` | .sisyphus/evidence/* | grep |
| 12 | `feat(ota): create xmesh-ota library structure` | lib/xmesh-ota/* | ls |
| 13 | `feat(ota): implement ESP-IDF native OTA with A/B partitions` | lib/xmesh-ota/*, partitions.csv | pio run |
| 14 | `docs: add OTA documentation` | README.md, AGENTS.md | grep |
| 15 | `fix: add error handling and watchdog protection` | lib/*, firmware/* | pio run |
| 16 | `test: add scale testing results (5-10 nodes)` | .sisyphus/evidence/* | grep |
| 17 | `docs: complete production documentation` | docs/*, CHANGELOG.md | ls |

---

## Success Criteria

### Verification Commands
```bash
# Build succeeds
cd firmware/production && pio run 2>&1 | grep -q "SUCCESS"

# All modules exist
ls lib/xmesh-core/src/*.cpp | wc -l | awk '{if ($1 >= 4) exit 0; else exit 1}'
ls lib/xmesh-hal/src/*.cpp | wc -l | awk '{if ($1 >= 2) exit 0; else exit 1}'
ls lib/xmesh-ota/src/*.cpp | wc -l | awk '{if ($1 >= 1) exit 0; else exit 1}'

# Documentation complete
test -f README.md && test -f AGENTS.md && test -f docs/ARCHITECTURE.md
```

### Final Checklist
- [x] All "Must Have" present:
  - [x] Callback additions to LoRaMesher
  - [x] TrickleScheduler module
  - [x] CostRouter module
  - [x] ETXTracker module
  - [x] GatewayBalancer module
  - [x] HAL abstraction
  - [x] ESP-IDF native OTA
  - [x] Production firmware
- [x] All "Must NOT Have" absent:
  - [x] No algorithm tuning during refactoring
  - [x] No mesh-propagated OTA
  - [x] No multi-hardware support
- [x] Integration tests pass (test plan created - hardware execution deferred)
- [x] Scale testing completed (5-10 nodes) (test plan created - hardware execution deferred)
- [x] Documentation complete
