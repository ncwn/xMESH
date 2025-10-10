# xMESH Flooding Protocol - Baseline Implementation

## Overview

This firmware implements a simple **flooding protocol** as the baseline for xMESH research. In flooding, every node rebroadcasts all received packets to all neighbors. This ensures maximum packet delivery but generates high network overhead.

**Key Features:**
- Role-based behavior (Sensor/Router/Gateway)
- Duplicate detection using a 5-entry circular cache
- Packet sequence numbers to prevent loops
- OLED display showing node status
- Heltec WiFi LoRa32 V3 support (ESP32-S3 + SX1262)

## Protocol Behavior

### Sensor Node (`ROLE_SENSOR`)
- Generates data packets every 60 seconds
- Broadcasts packets to all neighbors
- Rebroadcasts received packets (flooding)
- Tracks TX/RX packet counts

### Router Node (`ROLE_ROUTER`)
- Only forwards packets (no data generation)
- Rebroadcasts all received packets (flooding)
- Implements duplicate detection
- Tracks TX/RX packet counts

**Note:** Build flag is `XMESH_ROLE_ROUTER` (not `ROLE_ROUTER`) to avoid conflicts with LoRaMesher library.

### Gateway Node (`ROLE_GATEWAY`)
- Receives and logs packets
- **Does NOT rebroadcast** (sink node)
- Displays packet statistics
- Can be connected to Raspberry Pi for data collection

**Note:** Build flag is `XMESH_ROLE_GATEWAY` (not `ROLE_GATEWAY`) to avoid conflicts with LoRaMesher library.

## Duplicate Detection

To prevent infinite loops, the firmware uses a **5-entry circular cache** that stores:
- Source address
- Sequence number

When a packet arrives:
1. Check if (srcAddr, seqNum) exists in cache
2. If yes: Drop packet (duplicate)
3. If no: Process packet and add to cache

## Hardware Requirements

- **Heltec WiFi LoRa32 V3**
  - ESP32-S3 microcontroller
  - SX1262 LoRa transceiver
  - 0.96" OLED display (128x64)
  - Built-in antenna

## Display Layout

```
Line 1: ID:A3F2 [S]       # Node ID + Role (S/R/G)
Line 2: TX:45 RX:38       # Packet counts
Line 3: FLOOD             # Protocol indicator
Line 4: DC:0.0%           # Duty cycle (placeholder)
```

## Building the Firmware

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install)
2. Clone the xMESH repository

### Build Commands

```bash
cd firmware/1_flooding

# Build for Sensor node
pio run -e sensor

# Build for Router node
pio run -e router

# Build for Gateway node
pio run -e gateway
```

### Upload to Board

```bash
# Upload to sensor (specify port if needed)
pio run -e sensor -t upload --upload-port /dev/tty.usbserial-0001

# Upload to router
pio run -e router -t upload --upload-port /dev/tty.usbserial-0001

# Upload to gateway
pio run -e gateway -t upload --upload-port /dev/tty.usbserial-0001
```

**On macOS:** Port is typically `/dev/tty.usbserial-0001` or `/dev/tty.SLAB_USBtoUART`  
**On Linux:** Port is typically `/dev/ttyUSB0` or `/dev/ttyACM0`  
**On Windows:** Port is typically `COM3` or `COM4`

**To find your port:**
```bash
# macOS/Linux
ls /dev/tty.* | grep -i usb

# Or use PlatformIO
pio device list
```

### Monitor Serial Output

```bash
# Monitor serial output (115200 baud)
pio device monitor -b 115200

# Or combine upload + monitor
pio run -e sensor -t upload -t monitor
```

## Configuration

Hardware configuration is defined in `firmware/common/heltec_v3_config.h`:

- **LoRa Frequency:** 923.2 MHz (AS923 Thailand)
- **Spreading Factor:** 7
- **Bandwidth:** 125 kHz
- **TX Power:** 14 dBm
- **Packet Interval:** 60 seconds (sensors)
- **Duty Cycle Limit:** 1% (AS923 regulation)

## Packet Structure

```cpp
struct SensorData {
    uint32_t seqNum;        // Sequence number (duplicate detection)
    uint16_t srcAddr;       // Original source address
    uint32_t timestamp;     // Timestamp (ms since boot)
    float sensorValue;      // Simulated sensor data
    uint8_t hopCount;       // Number of hops from source
};
```

## Testing

### Step-by-Step: Flashing 3 Boards for Topology A

**What you need:**
- 3 Heltec WiFi LoRa32 V3 boards
- USB cable
- Computer with PlatformIO installed

**Instructions:**

1. **Flash Board 1 as SENSOR:**
   ```bash
   cd firmware/1_flooding
   
   # Connect Board 1 via USB
   # Find port: ls /dev/tty.* | grep -i usb
   
   pio run -e sensor -t upload --upload-port /dev/tty.usbserial-0001
   ```
   
   **Verify:** Display should show `ID:XXXX [S]` and TX counter incrementing

2. **Flash Board 2 as ROUTER:**
   ```bash
   # Disconnect Board 1, connect Board 2
   
   pio run -e router -t upload --upload-port /dev/tty.usbserial-0001
   ```
   
   **Verify:** Display should show `ID:YYYY [R]`

3. **Flash Board 3 as GATEWAY:**
   ```bash
   # Disconnect Board 2, connect Board 3
   
   pio run -e gateway -t upload --upload-port /dev/tty.usbserial-0001
   ```
   
   **Verify:** Display should show `ID:ZZZZ [G]`

4. **Power all 3 boards** (via USB or battery)

5. **Monitor gateway serial output:**
   ```bash
   pio device monitor -b 115200 -p /dev/tty.usbserial-0001
   ```

### Topology A (3-node linear): S1 → R1 → G1

1. Flash 3 boards:
   - Board 1: Sensor (`pio run -e sensor -t upload`)
   - Board 2: Router (`pio run -e router -t upload`)
   - Board 3: Gateway (`pio run -e gateway -t upload`)

2. Power on all boards

3. Expected behavior:
   - Sensor sends packet every 60s
   - Router rebroadcasts packet
   - Gateway receives and logs packet
   - All nodes show incrementing TX/RX counters

4. Monitor gateway serial output to verify packet delivery

### Serial Output Example

**Sensor:**
```
=================================
xMESH Flooding Protocol
Role: S (SENSOR)
=================================

LoRaMesher initialized
Local address: A3F2

TX: Seq=0 Value=42.35
TX: Seq=1 Value=67.12
```

**Router:**
```
=================================
xMESH Flooding Protocol
Role: R (ROUTER)
=================================

LoRaMesher initialized
Local address: B4E8

RX: Seq=0 From=A3F2 Hops=1 Value=42.35
FLOOD: Rebroadcasting packet 0 from A3F2
```

**Gateway:**
```
=================================
xMESH Flooding Protocol
Role: G (GATEWAY)
=================================

LoRaMesher initialized
Local address: C5D9

RX: Seq=0 From=A3F2 Hops=2 Value=42.35
GATEWAY: Packet 0 from A3F2 received (hops=2, value=42.35)
```

## Known Issues & Warnings

### ✅ RESOLVED: Display Not Working
**Problem:** OLED display remains blank on Heltec V3 board  
**Cause:** Heltec V3 requires GPIO 36 (Vext) to be set LOW to power the OLED  
**Solution:** Added Vext power control in display.cpp:
```cpp
pinMode(36, OUTPUT);
digitalWrite(36, LOW);  // LOW = OLED power ON
delay(100);
```
**Status:** ✅ FIXED - Display now works correctly

### ✅ RESOLVED: Role Naming Conflict
**Problem:** Router nodes were acting as gateways (not rebroadcasting)  
**Cause:** LoRaMesher library's `BuildOptions.h` defines `ROLE_GATEWAY` as a numeric constant (0b00000001), conflicting with our compile-time flags  
**Solution:** Renamed all role flags to use `XMESH_ROLE_*` prefix:
- `ROLE_SENSOR` → `XMESH_ROLE_SENSOR`
- `ROLE_ROUTER` → `XMESH_ROLE_ROUTER`
- `ROLE_GATEWAY` → `XMESH_ROLE_GATEWAY`

**Files Modified:**
- `platformio.ini` - Updated build flags
- `firmware/common/heltec_v3_config.h` - Updated role definitions and logic

**Status:** ✅ FIXED - All roles now work correctly, no compiler warnings

### Hardware Testing Status
**Date:** 2025-10-10  
**Status:** ✅ ALL 3 NODE TYPES TESTED AND WORKING

**Test Results:**
- **Sensor (ID: 6674):** TX=4, RX=2 ✅
- **Router (ID: D218):** TX=2, RX=2 ✅ (1:1 forwarding ratio)
- **Gateway:** TX=0, RX=2 ✅ (sink behavior confirmed)
- **Duplicate Detection:** Working correctly ✅
- **Display:** All nodes showing correct roles [S], [R], [G] ✅

## File Structure

```
firmware/1_flooding/
├── platformio.ini          # Build configuration
├── README.md              # This file
└── src/
    ├── main.cpp           # Main firmware logic
    ├── display.h          # Display interface
    └── display.cpp        # Display implementation

firmware/common/
└── heltec_v3_config.h     # Hardware pin definitions
```

## Performance Metrics (To Be Collected)

This baseline firmware will be tested to collect:
- **PDR (Packet Delivery Ratio):** Percentage of packets reaching gateway
- **Latency:** Time from sensor TX to gateway RX
- **Overhead:** Number of duplicate/forwarded packets
- **Duty Cycle:** LoRa airtime usage

Results will be compared against hop-count and gateway-aware protocols.

## Next Steps

1. Flash firmware to all 5 Heltec boards
2. Test Topology A (3-node linear)
3. Verify OLED display shows correct role and counts
4. Collect baseline metrics for comparison
5. Proceed to Week 3: Hop-Count Routing implementation

## Troubleshooting

### Display not working
**✅ RESOLVED:** Heltec V3 requires Vext pin (GPIO 36) set LOW to power OLED. This is now automatically handled in `display.cpp` initialization.

If display still doesn't work:
- Check I2C address is 0x3C
- Verify OLED_SDA=17, OLED_SCL=18, OLED_RST=21
- Ensure Vext (GPIO 36) is set to OUTPUT and LOW
- Check I2C is initialized: `Wire.begin(OLED_SDA, OLED_SCL)`

### Role not working correctly
**✅ RESOLVED:** Use `XMESH_ROLE_*` build flags (not `ROLE_*`) to avoid conflicts with LoRaMesher library.

Verify your build environment in `platformio.ini`:
```ini
[env:sensor]
build_flags = -D XMESH_ROLE_SENSOR

[env:router]
build_flags = -D XMESH_ROLE_ROUTER

[env:gateway]
build_flags = -D XMESH_ROLE_GATEWAY
```

### No LoRa packets received
- Verify all nodes on same frequency (923.2 MHz)
- Check antenna connections
- Ensure nodes are within range (~100m line-of-sight)
- Verify serial output shows "LoRaMesher initialized"

### Upload failed
- Press RESET button while uploading
- Check USB cable and port permissions
- Try `pio run -e sensor -t upload --upload-port /dev/ttyUSB0`
- On macOS, ensure USB serial drivers are installed

## References

- [Heltec WiFi LoRa32 V3 Docs](https://heltec.org/project/wifi-lora-32-v3/)
- [LoRaMesher Library](https://github.com/LoRaMesher/LoRaMesher)
- [xMESH Proposal](../../proposal_docs/st123843_internship_proposal.md)
- [Project README](../../README.md)

---

**Author:** xMESH Research Project  
**Date:** 2025-10-10  
**Status:** ✅ Week 1 - Baseline Implementation Complete & Hardware Tested

**Hardware Testing:** All 3 node types verified working on physical Heltec boards (2025-10-10)
