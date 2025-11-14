# Experiment Protocol & Testing Procedures

**Version:** November 13, 2025  
**Reference Logs:** See `experiments/FINAL_VALIDATION_TESTS.md` for every executed run and `experiments/results/<protocol>/<testname>/` for raw data.

---

## 1. Scope

| Area | Status | Notes |
|------|--------|-------|
| Indoor validation (3–5 nodes, 30 min) | ✅ Complete | 14 tests logged Nov 10‑13. Cite `experiments/FINAL_VALIDATION_TESTS.md`. |
| Dual-gateway (W5) | ✅ Complete | `w5_gateway_indoor_20251113_013301/` (cold start) + `w5_gateway_indoor_over_I600s_node_detection_20251113_120301/` (failure cycling). |
| Physical multi-hop (≤8 dBm) | ⚠️ Pending | Hallway/parking-lot campaign (Test 5). Required before claiming mesh behavior. |
| Optional ≥60 min soak | ⏳ Planned | Run after multi-hop test if time allows. |

---

## 2. Standard Test Setup

- **Environment:** Indoor table-top, ≤4 m spacing (use hallway/outdoor for multi-hop).  
- **Hardware:** 5× Heltec WiFi LoRa 32 V3 + Raspberry Pi border node.  
- **LoRa PHY (AS923 TH):** 923.2 MHz, 125 kHz BW, SF7, CR4/5, sync word 0x12, preamble 8.  
- **TX Power:** 10 dBm for indoor validation; 6‑8 dBm for hallway multi-hop runs.  
- **Traffic:** Sensors send one packet/minute (20 B payload).  
- **Duration:** 30 minutes per validation test; 35 minutes for endurance; ≥60 minutes optional soak.

---

## 3. Topologies & Roles

| Name | Layout | Purpose |
|------|--------|---------|
| 3-node linear | `[Sensor1]–[Relay3]–[Gateway5]` | Baseline functionality (all protocols). |
| 4-node linear (2 sensors) | `[S1]–[S2]–[R3]–[G5]` | Stress test with competing sensors (Protocols 1‑3). |
| 4-node diamond | `[S1]→{R3,R4}→[G5]` | Multi-path opportunity for Protocol 3. |
| 5-node linear | `[S1]–[S2]–[R3]–[R4]–[G5]` | Scalability + fault injection. |
| 5-node dual-gateway | `[S1]–[S2]–[R3]` feeding `[G4]` and `[G5]` | W5 load sharing (sensors logged). |
| Multi-hop hallway (pending) | `[S1]–(10‑15 m)–[R3]–(10‑15 m)–[G5]` | Forces `hops>0`, validates cost-based routing. |

> Use `experiments/FINAL_VALIDATION_TESTS.md` to map each topology to its log folder.

---

## 4. Execution Checklist

1. **Prepare hardware**
   - Flash boards via `firmware/<protocol>/flash_node.sh <NODE_ID> <PORT>`.
   - Verify OLED role/address, duty-cycle limit (1 %).
2. **Start capture**
   ```bash
   python3 raspberry_pi/multi_node_capture.py \
     --nodes 1,5 \
     --ports /dev/cu.usbserial-0001,/dev/cu.usbserial-7 \
     --duration 1800 \
     --output experiments/results/protocol3/30min_overhead_validation_20251111_032647
   ```
3. **Monitor**  
   - Gateway mandatory for PDR.  
   - Sensor logging required for W5 tests.  
   - Relay logging recommended for multi-hop proof.
4. **Annotate**  
   - Add `ANALYSIS.md` or summary to each test folder.  
   - Update `.context/session_log.md` and `experiments/FINAL_VALIDATION_TESTS.md`.

---

## 5. Physical Multi-Hop Campaign (Pending)

| Step | Details |
|------|---------|
| Objective | Demonstrate `hops>0` and cost-based route selection by spacing nodes ≥10 m apart with TX power 6‑8 dBm. |
| Topology | `[Sensor1]–[Relay3]–[Gateway5]` (optionally add `[Sensor2]` to create contention). |
| Location | Hallway, parking lot, or outdoor path with at least 20 m line-of-sight. |
| Monitoring | Gateway (routing table should show `hops=2`), relay (forwarding logs), optional sensor. |
| Deliverables | New folder under `experiments/results/protocol3/` with node logs, ANALYSIS.md, and photos/notes. Update README/PRD/CLAUDE once complete. |

---

## 6. Data Management

- **Folder structure:** `experiments/results/<protocol>/<testname_timestamp>/nodeX.log`.  
- **Naming convention:** `protocol3/w5_gateway_indoor_20251113_013301/`.  
- **Backups:** Archive original docs/tests under `docs/archive/<date>/` and `experiments/archive/<date>/` before rewriting summaries.  
- **Analysis:** Use `experiments/FINAL_VALIDATION_TESTS.md` as the single source of truth for the test matrix; avoid duplicating tables elsewhere.
