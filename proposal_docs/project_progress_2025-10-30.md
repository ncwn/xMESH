# xMESH Progress Brief – 30 Oct 2025

## Current Stage
- Phase 4, Step 2 in the roadmap – validating sequence-aware ETX and Trickle enhancements on the gateway-aware routing firmware.
- All three firmware roles (gateway, router, sensor) now use the callback-enabled LoRaMesher build.
- Latest code is synced on `main` (commit `41ca42c`).

## What Has Been Completed
- **Baseline protocols** remain stable: flooding and hopcount keep using the upstream LoRaMesher library for reference runs.
- **Trickle integration:**
  - Gateway HELLO scheduling now runs through Trickle callbacks.
  - Added safeguards: force first HELLO, cap suppression streaks at four, and trigger a safety HELLO if none have been sent in ≤5 minutes.
- **Sequence-aware ETX metrics:**
  - Gateway tracks packet sequence numbers, marks missing packets as failures, and resets cleanly after sensor restarts.
  - Extended `gateway_data_collection.py` to log ETX updates and notices (`--protocol seq-aware-etx`).
- **Bench tests:**
  - Cold-start Trickle run (`run2_gateway.csv`) confirmed interval growth to 600 s with only one HELLO and no packet loss.
  - Safety timer run (`seq-aware-etx/run1_gateway.csv`) captured induced packet gaps (ETX climbed to 1.15 with a 9/10 success window) and showed the forced HELLO after ~5 minutes of suppression.

## Pseudocode Highlights

### 1. Trickle Scheduler with Safety Timer
```
function trickle_get_schedule(timer):
    if Trickle disabled:
        return (shouldSend = true, waitMs = I_min)

    ensure timer started
    if interval elapsed:
        double interval up to I_max

    shouldSend = false
    if reached transmit point:
        if heard >= k consistent HELLOs and suppressions < limit:
            increment suppress counter
        else:
            shouldSend = true
            reset suppression counter
            record lastTransmitTime = now

    waitMs = time until next transmit point (clamped to [1, I_max])

    if has transmitted before AND shouldSend == false AND
       (now - lastTransmitTime) >= MAX_SILENCE:
        force shouldSend = true
        reset suppression counter
        record lastTransmitTime = now

    return (shouldSend, waitMs)
```

### 2. Sequence-Aware ETX Update
```
function update_link_metrics(addr, rssi, snr, seqNum):
    link = get_or_create_link(addr)
    update EMA for RSSI/SNR

    if first packet:
        mark success and store seqNum
        return

    expected = link.lastSeqNumber + 1

    if seqNum == link.lastSeqNumber:
        log duplicate, do nothing
    else if seqNum == expected:
        mark success, store seqNum
    else if seqNum > expected:
        missed = min(seqNum - expected, window_size)
        add `missed` failures to ETX window
        add success for current packet
        store seqNum
    else:
        // probable reset (wraparound or reboot)
        reset ETX window to defaults
        mark success for new seqNum

    recompute ETX using sliding window + EWMA
```

### 3. Data Collection Workflow
```
python gateway_data_collection.py \
    --port /dev/cu.usbserial-0001 \
    --protocol seq-aware-etx \
    --run-number N \
    --duration 15 \
    --output-dir ../experiments/results/new_bench

collector steps:
    open serial at 115200
    write CSV header (monitoring, packet, Trickle, ETX columns)
    loop for duration:
        read line
        if monitoring block → record channel/memory/queue stats
        if Trickle event → record transmit/double, suppression data
        if ETX update → log address, value, window details
        if ETX notice → log text (duplicates, resets, gaps)
        if packet line → log seq, source, hops
    print summary (duration, packet count, max duty-cycle)
```

## Next Actions
- Capture fresh `seq-aware-etx` runs with the updated firmware:
  1. Baseline (no loss).
  2. Induced loss (sensor stays powered, misses a few packets).
  3. Reset recovery (sensor/router reboot during capture).
- Feed new CSVs into analysis scripts and refresh figures once ETX behaviour is confirmed.
- Resume Phase 4 Step 3: feed ETX + cost metrics back into route selection.
