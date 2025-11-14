# Node ID Migration Guide

**Date:** November 14, 2025
**Branch:** feature/gps-pm-integration-sensor
**Purpose:** Document node ID renumbering and configuration cleanup

---

## Summary of Changes

### ✅ Node ID Configuration Update

**OLD Mapping (Before Nov 14, 2025):**
```
Node 1, 2: Sensors
Node 3:    Relay (only one)
Node 4, 5: Gateways
```

**NEW Mapping (After Nov 14, 2025):**
```
Node 1, 2: Sensors (with PM + GPS) - KEPT
Node 3, 4: Relays - EXTENDED (added Node 4)
Node 5, 6: Gateways - RENUMBERED (changed from 4,5)
```

**Reason for Change:**
- Support dual relay nodes (Node 3 & 4)
- Standardize gateway numbering at 5, 6 (not 4, 5)
- Clearer role grouping: 1-2 sensors, 3-4 relays, 5-6 gateways
- Aligns with PM + GPS sensor integration

---

## Code Changes Made

### 1. src/config.h

**Node ID Definitions:**
```cpp
// OLD
#define IS_SENSOR   (NODE_ID <= 2)              // Nodes 1, 2
#define IS_RELAY    (NODE_ID == 3)              // Node 3 only
#define IS_GATEWAY  (NODE_ID == 4 || NODE_ID == 5)  // Nodes 4, 5
#define GATEWAY_ADDRESS  0x0005

// NEW
#define IS_SENSOR   (NODE_ID == 1 || NODE_ID == 2)  // Nodes 1, 2 (KEPT)
#define IS_RELAY    (NODE_ID == 3 || NODE_ID == 4)  // Nodes 3, 4 (EXTENDED)
#define IS_GATEWAY  (NODE_ID == 5 || NODE_ID == 6)  // Nodes 5, 6 (RENUMBERED)
#define GATEWAY_ADDRESS  0x0005  // Unchanged (Node 5 is primary)
```

### 2. flash_node.sh

**Help Text Updated:**
```bash
# NEW
echo "  1, 2:  Sensor (with PM + GPS)"
echo "  3, 4:  Relay"
echo "  5, 6:  Gateway"
```

### 3. Files Removed

- ❌ `lib/heltec_v3_pins.h` (duplicate, conflicting power settings)
- ❌ `flash_5node_multihop_test.sh` (unused)
- ❌ `flash_low_power_test.sh` (unused)

### 4. Radio Power Consolidation

**Protocol 3 Now Uses:**
- ✅ `src/heltec_v3_pins.h`: Single source for all pin definitions + power settings
- ❌ Removed `lib/heltec_v3_pins.h` duplicate

**Protocols 1 & 2 Continue Using:**
- ✅ `firmware/common/heltec_v3_pins.h`: Shared pin definitions (14 dBm power)

---

## Impact on Testing

### Flash Commands

**OLD Commands:**
```bash
./flash_node.sh 1 /dev/cu.usbserial-0001  # Sensor
./flash_node.sh 2 /dev/cu.usbserial-5     # Sensor
./flash_node.sh 3 /dev/cu.usbserial-7     # Relay
./flash_node.sh 5 /dev/cu.usbserial-XXXX  # Gateway
```

**NEW Commands:**
```bash
./flash_node.sh 2 /dev/cu.usbserial-0001  # Sensor (with PM + GPS)
./flash_node.sh 3 /dev/cu.usbserial-5     # Sensor (with PM + GPS)
./flash_node.sh 4 /dev/cu.usbserial-7     # Relay
./flash_node.sh 6 /dev/cu.usbserial-XXXX  # Gateway
```

### Serial Output

**Node Addresses Will Show:**
```
OLD: 0001, 0002, 0003, 0005
NEW: 0002, 0003, 0004, 0006
```

### Previous Test Logs

All test logs from Nov 10-13, 2025 use OLD node IDs:
- Logs remain valid for historical reference
- Document as "using old ID mapping (1, 2, 3, 5)"
- New tests will use new mapping (2, 3, 4, 6)

---

## Backups

All original files backed up in:
```
firmware/3_gateway_routing/.backups_before_node_id_change/
├── config.h.backup
├── heltec_v3_pins.h.backup
└── flash_node.sh.backup
```

To revert changes:
```bash
cd firmware/3_gateway_routing
cp .backups_before_node_id_change/config.h.backup src/config.h
cp .backups_before_node_id_change/heltec_v3_pins.h.backup src/heltec_v3_pins.h
cp .backups_before_node_id_change/flash_node.sh.backup flash_node.sh
```

---

## Testing Checklist

After these changes, you MUST reflash all nodes:

### Node 2 (Sensor with PM + GPS):
```bash
./flash_node.sh 2 /dev/cu.usbserial-0001
# Verify: Local Address: 0002
# Verify: Role: SENSOR (SENSOR)
# Verify: PM and GPS sensors initialize
```

### Node 3 (Sensor with PM + GPS):
```bash
./flash_node.sh 3 /dev/cu.usbserial-5
# Verify: Local Address: 0003
# Verify: Role: SENSOR (SENSOR)
```

### Node 4 (Relay):
```bash
./flash_node.sh 4 /dev/cu.usbserial-7
# Verify: Local Address: 0004
# Verify: Role: RELAY
```

### Node 6 (Gateway):
```bash
./flash_node.sh 6 /dev/cu.usbserial-XXXX
# Verify: Local Address: 0006
# Verify: Role: GATEWAY (GATEWAY)
# Verify: Receives packets from sensors
```

### Expected Routing Table at Gateway:
```
Node 0002: via 0002, metric=1 hop, role=SENSOR
Node 0003: via 0003, metric=1 hop, role=SENSOR
Node 0004: via 0004, metric=1 hop, role=RELAY
```

---

## Radio Power Settings

### Protocol 3 (src/heltec_v3_pins.h):
```cpp
#define DEFAULT_LORA_TX_POWER  2  // dBm (indoor/multi-hop testing)
```

To change power, edit ONLY this file:
```
firmware/3_gateway_routing/src/heltec_v3_pins.h
```

**Clean build required after power change:**
```bash
pio run -t clean
pio run
```

### Protocols 1 & 2 (firmware/common/heltec_v3_pins.h):
```cpp
#define DEFAULT_LORA_TX_POWER  14  // dBm
```

Protocols 1 and 2 use shared common/ file.

---

## Build Validation

**✅ Compilation successful:**
- RAM: 6.8% (22,432 bytes)
- Flash: 32.5% (425,525 bytes)
- No errors or warnings
- All sensors integrated correctly

---

**Migration Complete!**
**Next:** Reflash all nodes with new IDs and test PM + GPS data transmission.

