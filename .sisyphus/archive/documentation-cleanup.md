# xMESH Documentation Cleanup & Consolidation

## TL;DR

> **Quick Summary**: Clean up .sisyphus directory by archiving completed plans, update PROJECT_MEMORY.md with verified line counts and accurate file paths, and fix AGENTS.md path errors.
> 
> **Deliverables**:
> - Updated PROJECT_MEMORY.md with accurate line counts and paths
> - Updated AGENTS.md with correct file locations and missing modules
> - Archived historical plans in .sisyphus/archive/
> - Updated boulder.json with completion status
> - Cleaned .sisyphus/ root directory
> 
> **Estimated Effort**: Quick (~30 minutes)
> **Parallel Execution**: YES - 2 waves
> **Critical Path**: Task 1 (create archive) → Tasks 2-4 (parallel updates) → Task 5 (cleanup)

---

## Context

### Original Request
Consolidate and clean up the .sisyphus directory after successful completion of all xMESH implementation plans. Update documentation to reflect verified implementation state.

### Verified Implementation State (from user's multi-agent analysis)
All 6 plan files have been fully completed and verified:
- xmesh-production-refactor.md (17 tasks) - COMPLETE
- sensor-integration.md (10 tasks) - COMPLETE
- mobility-aware-trickle.md (6 tasks) - COMPLETE
- xmesh-features.md - COMPLETE
- serial-ota-stability.md (8 tasks) - COMPLETE
- xmesh-cleanup.md (14 tasks) - COMPLETE

### Key Discrepancies Found
1. **PROJECT_MEMORY.md line 155-157**: States `include/xmesh/ota/` but actual path is `include/ota/`
2. **PROJECT_MEMORY.md line counts**: Outdated (e.g., OTAManager 278 → verified 407)
3. **AGENTS.md**: Missing MobilityDetector, SensorPacket modules; simplified paths

---

## Work Objectives

### Core Objective
Consolidate project knowledge into accurate, fact-checked documentation and clean up the .sisyphus directory structure.

### Concrete Deliverables
- `.sisyphus/archive/` - New directory containing all historical plan files
- `PROJECT_MEMORY.md` - Updated with verified line counts and corrected paths
- `AGENTS.md` - Updated with accurate file locations and complete module list
- `boulder.json` - Updated with completion status
- Clean `.sisyphus/` root - Only essential files remain

### Definition of Done
- [ ] `ls .sisyphus/` shows: PROJECT_MEMORY.md, boulder.json, evidence/, archive/
- [ ] `grep "include/ota/" PROJECT_MEMORY.md` returns matches (path corrected)
- [ ] `grep "MobilityDetector" AGENTS.md` returns matches (module added)
- [ ] `cat .sisyphus/boulder.json | grep completed` returns match

### Must Have
- Accurate line counts from verified analysis
- Correct file paths matching actual filesystem structure
- Preserved historical plans (archived, not deleted)
- Working references to documentation

### Must NOT Have (Guardrails)
- Do NOT delete evidence/ folder or its contents
- Do NOT modify any source code files
- Do NOT change implementation behavior
- Do NOT lose historical plan information (archive, don't delete)

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: N/A (documentation task)
- **User wants tests**: Manual verification
- **Framework**: N/A

### Automated Verification (Bash commands)

All tasks use simple bash verification - no user intervention required.

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately):
└── Task 1: Create archive directory and move plans

Wave 2 (After Wave 1):
├── Task 2: Update PROJECT_MEMORY.md (independent)
├── Task 3: Update AGENTS.md (independent)
└── Task 4: Update boulder.json (independent)

Wave 3 (After Wave 2):
└── Task 5: Final cleanup (delete notepads, PROJECT_SUMMARY.md)

Critical Path: Task 1 → Task 5
Parallel Speedup: Tasks 2-4 run in parallel
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 1 | None | 5 | None |
| 2 | None | None | 3, 4 |
| 3 | None | None | 2, 4 |
| 4 | None | None | 2, 3 |
| 5 | 1, 2, 3, 4 | None | None (final) |

---

## TODOs

- [ ] 1. Create archive directory and move historical plans

  **What to do**:
  - Create `.sisyphus/archive/` directory
  - Move all 6 plan files from `.sisyphus/plans/` to `.sisyphus/archive/`
  - Keep `.sisyphus/plans/` directory empty (for future plans)

  **Files to archive**:
  - xmesh-production-refactor.md
  - sensor-integration.md
  - mobility-aware-trickle.md
  - xmesh-features.md
  - serial-ota-stability.md
  - xmesh-cleanup.md

  **Must NOT do**:
  - Do NOT delete the plan files (move only)
  - Do NOT archive evidence/ folder

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple file system operations, no complex logic
  - **Skills**: None required
    - Simple bash commands sufficient

  **Parallelization**:
  - **Can Run In Parallel**: NO (must complete first)
  - **Parallel Group**: Wave 1 (alone)
  - **Blocks**: Task 5
  - **Blocked By**: None

  **References**:
  - `.sisyphus/plans/` - Source directory (verified via glob)
  - Current files: xmesh-production-refactor.md, sensor-integration.md, mobility-aware-trickle.md, xmesh-features.md, serial-ota-stability.md, xmesh-cleanup.md

  **Acceptance Criteria**:
  ```bash
  # Verify archive created and populated
  ls -la .sisyphus/archive/
  # Assert: Shows 6 .md files
  
  # Verify plans directory is empty (ready for future plans)
  ls .sisyphus/plans/ 2>/dev/null | wc -l
  # Assert: 0 (or directory can be deleted)
  ```

  **Commit**: NO (groups with Task 5)

---

- [ ] 2. Update PROJECT_MEMORY.md with verified data

  **What to do**:
  - Update line counts to verified values from analysis:
    ```
    xmesh-core components:
    - CostRouter: 166 lines (was 74)
    - TrickleScheduler: 295 lines (was 162)
    - ETXTracker: 271 lines (was 166)
    - GatewayBalancer: 484 lines (was 249)
    - MobilityDetector: 240 lines (was 177)
    - MeshConfig: 50 lines (new addition)
    Total xmesh-core: ~1,506 lines

    xmesh-hal components:
    - Display: 248 lines (was 142)
    - Sensors: 296 lines (was 208)
    - SensorPacket: 44 lines (already listed)
    Total xmesh-hal: ~618 lines (was ~350)

    xmesh-ota components:
    - OTAManager: 407 lines (was 278)
    - VersionControl: 147 lines (was 37)
    Total xmesh-ota: ~572 lines (was ~315)

    Firmware:
    - main.cpp: 958 lines (was 957)
    ```
  - Fix path error on line 155-157:
    - Change `include/xmesh/ota/` to `include/ota/`
  - Update total project lines: ~3,654 lines (was ~2,450)
  - Update Serial CLI command count to 27 (was 15+)

  **Must NOT do**:
  - Do NOT change the document structure
  - Do NOT remove any sections
  - Do NOT modify "What Is NOT Implemented" section incorrectly

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple text edits with clear before/after values
  - **Skills**: None required
    - Direct file editing sufficient

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 3, 4)
  - **Blocks**: Task 5 (implicitly)
  - **Blocked By**: None

  **References**:
  - `.sisyphus/PROJECT_MEMORY.md` - Target file
  - User's analysis (provided in request) - Source of truth for line counts
  - Lines 51-66: Component table to update
  - Lines 155-157: Path error to fix

  **Acceptance Criteria**:
  ```bash
  # Verify OTA path corrected
  grep "include/ota/" .sisyphus/PROJECT_MEMORY.md
  # Assert: Returns matches (path corrected)
  
  # Verify OTAManager line count updated
  grep "OTAManager.*407" .sisyphus/PROJECT_MEMORY.md
  # Assert: Returns match
  
  # Verify main.cpp line count updated
  grep "main.cpp.*958" .sisyphus/PROJECT_MEMORY.md
  # Assert: Returns match
  
  # Verify total updated
  grep "3,654" .sisyphus/PROJECT_MEMORY.md
  # Assert: Returns match (or similar total ~3,600)
  ```

  **Commit**: NO (groups with Task 5)

---

- [ ] 3. Update AGENTS.md with accurate file locations

  **What to do**:
  - Add missing modules to xmesh-core section:
    - `MobilityDetector.h/cpp`: SNR-based mobility state machine
  - Add missing modules to xmesh-hal section:
    - `SensorPacket.h`: Mesh packet structure for sensor data
  - Fix paths to show actual include structure:
    ```
    xmesh-core: lib/xmesh-core/include/xmesh/*.h
    xmesh-hal: lib/xmesh-hal/include/xmesh/hal/*.h
    xmesh-ota: lib/xmesh-ota/include/ota/*.h
    ```
  - Optionally add line counts for reference

  **Must NOT do**:
  - Do NOT change the overview or objectives sections
  - Do NOT modify key conventions
  - Do NOT remove existing accurate information

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple text additions to existing documentation
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 4)
  - **Blocks**: Task 5 (implicitly)
  - **Blocked By**: None

  **References**:
  - `AGENTS.md` - Target file (lines 13-29 for module sections)
  - `lib/xmesh-core/include/xmesh/` - Actual path structure (verified)
  - `lib/xmesh-hal/include/xmesh/hal/` - Actual path structure (verified)
  - `lib/xmesh-ota/include/ota/` - Actual path structure (verified)

  **Acceptance Criteria**:
  ```bash
  # Verify MobilityDetector added
  grep "MobilityDetector" AGENTS.md
  # Assert: Returns match
  
  # Verify SensorPacket added
  grep "SensorPacket" AGENTS.md
  # Assert: Returns match
  
  # Verify correct xmesh-core path mentioned
  grep -E "include/xmesh/.*\.h" AGENTS.md
  # Assert: Returns matches showing correct path
  ```

  **Commit**: NO (groups with Task 5)

---

- [ ] 4. Update boulder.json with completion status

  **What to do**:
  - Update boulder.json to reflect completed state:
    ```json
    {
      "active_plan": null,
      "completed_plan": "/Volumes/xMESH/xMESH/.sisyphus/archive/xmesh-production-refactor.md",
      "status": "completed",
      "started_at": "2026-01-28T21:18:14.045Z",
      "completed_at": "2026-01-30T00:00:00.000Z",
      "session_ids": ["ses_3f9dbf2c5ffe3U2RfSxjNHqhcn"],
      "plan_name": "xmesh-production-refactor",
      "summary": "All 6 plans completed: production refactor, sensor integration, mobility detection, features, OTA stability, cleanup"
    }
    ```

  **Must NOT do**:
  - Do NOT delete the file
  - Do NOT lose historical session information

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single JSON file update
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 2, 3)
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `.sisyphus/boulder.json` - Target file
  - Current content: shows active_plan pointing to old path

  **Acceptance Criteria**:
  ```bash
  # Verify completed status
  cat .sisyphus/boulder.json | grep '"status": "completed"'
  # Assert: Returns match
  
  # Verify completed_at date present
  cat .sisyphus/boulder.json | grep "completed_at"
  # Assert: Returns match
  
  # Verify active_plan is null
  cat .sisyphus/boulder.json | grep '"active_plan": null'
  # Assert: Returns match
  ```

  **Commit**: NO (groups with Task 5)

---

- [ ] 5. Final cleanup - remove obsolete files

  **What to do**:
  - Delete `.sisyphus/notepads/` directory (recursive)
  - Delete `.sisyphus/PROJECT_SUMMARY.md` (redundant with PROJECT_MEMORY.md)
  - Optionally delete `.sisyphus/plans/` directory if empty (or keep for future use)
  - Verify final .sisyphus structure

  **Final expected structure**:
  ```
  .sisyphus/
  ├── PROJECT_MEMORY.md      # Master project knowledge
  ├── boulder.json           # Completion marker
  ├── archive/               # Historical plans (6 files)
  │   ├── xmesh-production-refactor.md
  │   ├── sensor-integration.md
  │   ├── mobility-aware-trickle.md
  │   ├── xmesh-features.md
  │   ├── serial-ota-stability.md
  │   └── xmesh-cleanup.md
  └── evidence/              # Test evidence (preserved)
      ├── stability-test-20260129.md
      ├── stability-test-procedure.md
      ├── stability-test.md
      ├── integration-test.log
      └── scale-test-results.md
  ```

  **Must NOT do**:
  - Do NOT delete evidence/ folder
  - Do NOT delete PROJECT_MEMORY.md
  - Do NOT delete boulder.json
  - Do NOT delete archived plans

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple file deletion and verification
  - **Skills**: None required

  **Parallelization**:
  - **Can Run In Parallel**: NO (final task)
  - **Parallel Group**: Wave 3 (alone)
  - **Blocks**: None (final)
  - **Blocked By**: Tasks 1, 2, 3, 4

  **References**:
  - `.sisyphus/notepads/` - To delete
  - `.sisyphus/PROJECT_SUMMARY.md` - To delete

  **Acceptance Criteria**:
  ```bash
  # Verify notepads deleted
  ls .sisyphus/notepads/ 2>&1 | grep -q "No such file"
  # Assert: Directory does not exist
  
  # Verify PROJECT_SUMMARY deleted
  ls .sisyphus/PROJECT_SUMMARY.md 2>&1 | grep -q "No such file"
  # Assert: File does not exist
  
  # Verify final structure
  ls -la .sisyphus/
  # Assert: Shows PROJECT_MEMORY.md, boulder.json, archive/, evidence/
  
  # Verify archive contents
  ls .sisyphus/archive/ | wc -l
  # Assert: 6 files
  
  # Verify evidence preserved
  ls .sisyphus/evidence/ | wc -l
  # Assert: 5 files
  ```

  **Commit**: YES
  - Message: `docs: consolidate sisyphus directory, archive completed plans`
  - Files: `.sisyphus/*`
  - Pre-commit: N/A (documentation only)

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 5 | `docs: consolidate sisyphus directory, archive completed plans` | .sisyphus/*, AGENTS.md | ls .sisyphus/ shows clean structure |

---

## Success Criteria

### Verification Commands
```bash
# Final structure check
ls .sisyphus/
# Expected: PROJECT_MEMORY.md boulder.json archive evidence

# Archive complete
ls .sisyphus/archive/ | wc -l
# Expected: 6

# Evidence preserved
ls .sisyphus/evidence/ | wc -l
# Expected: 5

# Key fixes applied
grep "MobilityDetector" AGENTS.md && echo "AGENTS.md updated"
grep "include/ota/" .sisyphus/PROJECT_MEMORY.md && echo "Path fixed"
grep "completed" .sisyphus/boulder.json && echo "Boulder marked complete"
```

### Final Checklist
- [ ] All 6 plans archived in .sisyphus/archive/
- [ ] PROJECT_MEMORY.md has verified line counts
- [ ] PROJECT_MEMORY.md has correct OTA path (include/ota/)
- [ ] AGENTS.md includes MobilityDetector and SensorPacket
- [ ] boulder.json shows completed status
- [ ] notepads/ directory removed
- [ ] PROJECT_SUMMARY.md removed
- [ ] evidence/ folder preserved intact
