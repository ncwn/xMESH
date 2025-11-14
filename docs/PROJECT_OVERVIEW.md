# Project Overview & Research Objectives

**Version:** November 15, 2025 (`feature/gps-pm-integration-sensor`, latest: 3ae66a3)
**Source:** Condensed from the 97-page proposal and field notes for day-to-day reference.
**Major Update:** GPS+PM sensor integration and LOCAL fault isolation validation complete.

---

## 1. Mission

Design, implement, and empirically validate a gateway-aware cost routing protocol for LoRa mesh networks that:
- Reduces control overhead versus flooding and hop-count baselines.
- Maintains >95 % packet delivery ratio (PDR) on real ESP32-S3 + SX1262 hardware.
- Scales across 3–5 indoor nodes today and extends to hallway/outdoor deployments next.

---

## 2. Success Criteria (Current Status)

| Metric | Target | Status (Nov 13) | Evidence |
|--------|--------|-----------------|----------|
| Control overhead | ≥30 % reduction vs Protocol 2 | ✅ 31 % reduction with 180 s safety HELLO | `experiments/archive/3node_30min_overhead_validation_20251111_032647/` |
| PDR | >95 % across all protocols | ✅ 96.7‑100 % (Protocols 1‑3) | See `experiments/FINAL_VALIDATION_TESTS.md` |
| Fault detection | <7 minutes | ✅ 378 s (two missed safety HELLOs) | `experiments/archive/detection_180s_relay_shutdown_20251111_025043/`, `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` |
| Gateway load sharing | Balanced traffic when ≥2 gateways | ✅ `[W5] Load-biased gateway selection …` logs, 16 vs 13 packets | `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/` |
| Multi-hop routing | Demonstrate `hops>0` | ⚠️ Pending (requires hallway/parking-lot spacing at ≤8 dBm) | Upcoming Test 5 in `experiments/FINAL_VALIDATION_TESTS.md` |

---

## 3. Protocol Portfolio

| Protocol | Purpose | Key Behaviors | Validation Snapshot |
|----------|---------|---------------|---------------------|
| Protocol 1 – Flooding | O(N²) broadcast baseline | Controlled flooding (relay-only rebroadcast, TTL, duplicate cache) | 30 min linear test: 96.7 % PDR (`protocol1/4node_linear_20251111_175245/`) |
| Protocol 2 – Hop-Count | LoRaMesher default | Fixed 120 s HELLO, hop-count routing, all nodes forward if on path | 30 min linear test: 81.7 % PDR worst case; diamond test: 96.7 % PDR |
| Protocol 3 – Gateway-Aware | Proposed solution | Trickle HELLOs (60‑600 s + 180 s safety), multi-metric cost (W1‑W5), sequence-gap ETX, W5 load bias + override, proactive fault removal | 31 % overhead reduction + dual-gateway endurance logs (Nov 13) |

### Key Innovations
1. **Trickle-Driven HELLO Scheduling** – 85.7-90.9% efficiency validated (efficiency improves with network maturity).
2. **LOCAL Fault Isolation** – Fault impact ~10-30% of network (not 100%). Stable nodes: 90.9%, affected nodes: 66.7%. Validated Nov 14-15.
3. **W5 Active Load Sharing** – 65/35 traffic split across dual gateways validated Nov 14 (19 + ~10 packets).
4. **GPS + PM Environmental Monitoring** – PMS7003 + NEO-M8M integrated. 19+ PM readings, GPS coordinates transmitted. Nov 14-15.
5. **Zero-Overhead ETX** – Sequence-gap detection (superior to ACK-based methods).
6. **Fast Fault Detection** – 378s with immediate route removal.
---

## 4. Validation Snapshot

- **Indoor suite complete:** 14 × 30‑minute runs across 3‑5 node topologies; summarized in `experiments/FINAL_VALIDATION_TESTS.md`.  
- **Multi-gateway suite complete:** `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_20251113_013301/` (balanced load) and `experiments/results/protocol3/protocol3_validation_suite_20251113/w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` (fault cycling).  
- **Pending:** hallway/parking-lot multi-hop test (≤8 dBm, ≥10 m spacing) to prove `hops>0` and unlock comparative traffic-reduction metrics versus flooding.

---

## 5. Next Research Actions

1. **Physical Multi-Hop Test** – Capture logs showing `hops=2`, update README/PRD/CLAUDE/experiments docs, and add ANALYSIS.md + plots.  
2. **Documentation Polish** – Finish professional rewrites of remaining ANALYSIS files and thesis chapters.  
3. **Optional Soak/Outdoor Run** – ≥60 min dual-gateway repeat for thesis appendices or demonstration videos.  
4. **Open-Source Release Plan** – Package firmware/docs after hallway test; prepare LoRaMesher PR containing HELLO gateway-load serialization.

---

For detailed experimental procedures see `docs/EXPERIMENT_PROTOCOL.md`. For build/flash instructions see `docs/FIRMWARE_GUIDE.md` and `docs/HARDWARE_SETUP.md`.
