# Heltec LoRa 32 V3 Example with OLED Display

This example demonstrates how to use LoRaMesher with the **Heltec LoRa 32 V3** board including OLED display support.

## Hardware Specifications

- **Board**: Heltec WiFi LoRa 32 V3
- **LoRa Chip**: SX1262
- **MCU**: ESP32-S3FN8
- **Display**: 0.96" OLED (128x64, SSD1306)
- **Frequency**: 915 MHz (US915/AS923 compatible)

## Pin Configuration

### LoRa Pins

| Function | GPIO Pin | Note |
|----------|----------|------|
| CS (NSS) | 8        | SPI Chip Select |
| DIO1 (IRQ)| 14      | Interrupt pin |
| RST      | 12       | Reset pin |
| BUSY     | 13       | BUSY status (loraIo1 param) |
| MOSI     | 10       | SPI Data Out |
| MISO     | 11       | SPI Data In |
| SCK      | 9        | SPI Clock |

**Important:** For SX1262, the `loraIo1` configuration parameter should be set to the BUSY pin (GPIO 13), not DIO1.

### Display Pins (I2C)

| Function | GPIO Pin |
|----------|----------|
| SDA      | 17       |
| SCL      | 18       |
| RST      | 21       |

### Other

| Function | GPIO Pin | Note |
|----------|----------|------|
| LED      | 35       | Status LED |
| Vext     | 36       | Power for OLED/LoRa (must be LOW) |

**Critical:** GPIO 36 (Vext) must be set to LOW before initializing the display or LoRa module to provide power to peripherals.

## Features

This example implements:
- Counter broadcasting every 20 seconds
- Receiving and displaying packets from other nodes
- OLED display showing:
  - **Line 1**: Local node address (e.g., `Addr: 0x6674`)
  - **Line 2**: Current transmission status (e.g., `TX: Counter #6`)
  - **Line 3**: Number of discovered nodes (e.g., `Nodes: 4`)
  - **Line 4**: Received packet info (e.g., `RX: Counter #10`)
- LED indication for transmitted packets
- Automatic mesh network formation
- Distance-vector routing protocol

## Mesh Network Behavior

### Single Node
- Display shows `Nodes: 0`
- Broadcasts counter packets every 20 seconds
- No packets received (only local node)

### Multiple Nodes (2-5 or more)
- **Automatic Discovery**: Nodes automatically find each other within LoRa range
- **Routing Table**: Each node builds a routing table of discovered nodes
- **Display Updates**: `Nodes: X` shows the number of nodes in the routing table
- **Packet Reception**: When receiving packets, display shows:
  - Line 3: `From: 0xXXXX` (source address)
  - Line 4: `RX: Counter #X` (received counter value)

### Example with 5 Nodes
```
Node 1 (0x6674): Nodes: 4  ← Can see all other 4 nodes
Node 2 (0x1234): Nodes: 4  ← Can see all other 4 nodes
Node 3 (0x5678): Nodes: 4  ← Can see all other 4 nodes
Node 4 (0x9ABC): Nodes: 4  ← Can see all other 4 nodes
Node 5 (0xDEF0): Nodes: 4  ← Can see all other 4 nodes
```

Each node will:
- Receive counter packets from all other nodes
- Display the sender's address when receiving
- Update the routing table automatically
- Forward packets in multi-hop scenarios (if needed)

## Usage

### Flashing Multiple Boards

1. Flash the same code to all Heltec V3 boards
2. Each board will auto-generate a unique address from its MAC address
3. Power on all boards
4. Nodes will automatically discover each other (within ~30-60 seconds)
5. Watch the `Nodes: X` count increase on the display

### Single Board Testing

1. Open this project in PlatformIO
2. Build and upload to your Heltec LoRa 32 V3
3. The OLED display will show:
   - `Addr: 0xXXXX` (your unique address)
   - `TX: Counter #X` (incrementing every 20 seconds)
   - `Nodes: 0` (no other nodes detected)
4. Open Serial Monitor (115200 baud) for detailed logs

### Multi-Node Mesh Testing

1. Flash the same code to 2 or more boards
2. Place boards within LoRa range (typically 1-10 km depending on conditions)
3. Power on all boards
4. Observe the displays:
   - `Nodes: X` will increase as nodes discover each other
   - `From: 0xXXXX` will show when receiving packets
   - `RX: Counter #X` displays the received counter value
5. Serial monitors will show detailed routing information

## Troubleshooting

### Display Not Working
- **Check Vext**: GPIO 36 must be set LOW in setup()
- **Check I2C Pins**: SDA=17, SCL=18, RST=21
- **Power Cycle**: Reset the board if display doesn't initialize

### LoRa Error -705
- **Check BUSY Pin**: `loraIo1` must be set to GPIO 13 (BUSY), not GPIO 14 (DIO1)
- **Check SPI Pins**: SCK=9, MISO=11, MOSI=10, CS=8
- **Frequency**: Ensure 915 MHz is supported by your hardware variant

### Nodes Not Finding Each Other
- **Same Frequency**: All nodes must use the same frequency (915 MHz)
- **Same Sync Word**: All nodes must use `0x12` sync word
- **Within Range**: Nodes must be within LoRa communication range
- **Wait Time**: Allow 30-60 seconds for initial discovery
- **Check Antennas**: Ensure LoRa antennas are properly connected

### Display Shows "Nodes: 0"
- **Normal for Single Board**: This is expected with only one board
- **Flash Other Boards**: Upload the same code to additional boards
- **Check Transmission**: Verify "Sending packet #X" appears in serial monitor
- **Antenna Check**: Ensure the LoRa antenna is connected

## Serial Monitor Output

Expected output when running:
```
========================================
Heltec V3 LoRaMesher with Display
========================================

Initializing LoRaMesher for Heltec LoRa 32 V3...
LoRaMesher initialized successfully!
Local address: 0x6674
Sending packet #0
Sending packet #1
Sending packet #2
...
```

When receiving packets from other nodes:
```
Packet arrived from 1234 with size 4
Hello Counter received nº 5
Queue receiveUserData size: 1
```

## Notes

- **Frequency**: Currently set to 915 MHz (compatible with US915/AS923)
  - For Thailand: 915 MHz works (AS923 band ideal frequency is 920-925 MHz, but hardware may only support 868/915 MHz)
  - For Europe: Change to 868 MHz
  - For US: 915 MHz is correct
- **Unique Addresses**: Each board automatically generates a unique address from its MAC address
- **Mesh Formation**: Multiple boards will form a mesh network automatically
- **No Configuration Needed**: All nodes use the same code - no manual address assignment required
- **Range**: LoRa range depends on environment (1-2 km urban, 5-10 km rural, line of sight up to 15-20 km)
- **Sync Word**: Private network uses `0x12` - all nodes must use the same sync word to communicate
- **Display Auto-Updates**: The OLED display updates automatically in a background task

## Configuration Summary

The working configuration for Heltec LoRa 32 V3:
```cpp
config.loraCs = 8;      // NSS/CS
config.loraIrq = 14;    // DIO1 (interrupt)
config.loraRst = 12;    // Reset
config.loraIo1 = 13;    // BUSY pin (NOT DIO1!)
config.freq = 915.0;    // Frequency in MHz
config.bw = 125.0;      // Bandwidth 125 kHz
config.sf = 7;          // Spreading Factor 7
config.syncWord = 0x12; // Private sync word
```

**Critical Fix**: The `loraIo1` parameter must be set to the BUSY pin (GPIO 13), not DIO1 (GPIO 14). This is specific to the SX1262 chip and RadioLib's Module constructor.
