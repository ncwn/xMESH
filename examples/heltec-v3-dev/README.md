# Heltec WiFi LoRa 32 V3 Development

This folder contains the Heltec WiFi LoRa 32 V3 implementation and development environment, separate from the main LoRaMesher library.

## 📁 Folder Structure

```
heltec-v3-dev/
├── examples/
│   └── HeltecV3-Display/     # Main working example with OLED display
├── docs/                      # All Heltec V3 documentation
└── README.md                  # This file
```

## 🎯 Quick Start

### Hardware Requirements
- **Heltec WiFi LoRa 32 V3** board(s)
- USB-C cable
- LoRa antenna (mandatory - do not transmit without antenna!)

### Working Example

The **HeltecV3-Display** example is fully functional and tested:

```bash
cd examples/HeltecV3-Display
pio run -t upload -t monitor
```

## ✅ Verified Working Configuration

### Hardware Specifications
- **MCU**: ESP32-S3FN8
- **LoRa Chip**: SX1262
- **Display**: 0.96" OLED (128x64, SSD1306)
- **Frequency**: 915 MHz (US915/AS923 compatible)

### Critical Pin Configuration

**LoRa SX1262 Pins:**
| Function | GPIO | Note |
|----------|------|------|
| CS (NSS) | 8    | SPI Chip Select |
| DIO1 (IRQ) | 14 | Interrupt pin |
| RST      | 12   | Reset |
| BUSY     | 13   | **Important: loraIo1 parameter** |
| MOSI     | 10   | SPI Data Out |
| MISO     | 11   | SPI Data In |
| SCK      | 9    | SPI Clock |

**OLED Display Pins:**
| Function | GPIO |
|----------|------|
| SDA      | 17   |
| SCL      | 18   |
| RST      | 21   |

**Other:**
| Function | GPIO | Note |
|----------|------|------|
| LED      | 35   | Status LED |
| Vext     | 36   | **Must be LOW to power OLED/LoRa** |

### Critical Implementation Notes

1. **BUSY Pin Configuration**: The `loraIo1` parameter must be set to GPIO 13 (BUSY), not GPIO 14 (DIO1). This is specific to SX1262 and RadioLib.

2. **Vext Power**: GPIO 36 must be set LOW before initializing display or LoRa to provide power to peripherals.

3. **Custom SPI**: Heltec V3 uses non-standard SPI pins requiring custom SPI configuration.

## 🚀 Features

The HeltecV3-Display example implements:
- ✅ Working LoRa mesh networking
- ✅ OLED display with real-time status
- ✅ Automatic node discovery
- ✅ Distance-vector routing
- ✅ Multi-node communication (tested with 2+ nodes)
- ✅ Clean display layout (4 lines, no overlap)

### Display Shows:
```
0x6674        (Line 1: Node address)
TX: #15       (Line 2: Transmission counter)
RX: #12       (Line 3: Last received counter)
Nodes: 2      (Line 4: Number of discovered nodes)
```

## 📚 Documentation

Detailed documentation is available in the `docs/` folder:

### Quick References
- **[Quick Reference](docs/HELTEC_V3_QUICK_REF.md)** - Pin definitions and code snippets
- **[Setup Checklist](docs/HELTEC_V3_SETUP_CHECKLIST.md)** - Interactive verification checklist

### Comprehensive Guides
- **[Complete Guide](docs/HELTEC_V3_GUIDE.md)** - Full implementation guide
- **[Branch Summary](docs/HELTEC_V3_BRANCH_SUMMARY.md)** - Development history and fixes
- **[Migration Guide](docs/MIGRATION_TO_HELTEC_V3.md)** - Migrating from other boards

### Technical Documentation
- **[Examples Documentation](docs/HELTEC_V3_EXAMPLES.md)** - Example descriptions
- **[Project Structure](docs/PROJECT_STRUCTURE_EXPLAINED.md)** - Architecture explanation
- **[Implementation Complete](docs/IMPLEMENTATION_COMPLETE.md)** - Completion summary

## 🔧 Configuration Summary

```cpp
// Working configuration for Heltec LoRa 32 V3
config.loraCs = 8;      // NSS/CS
config.loraIrq = 14;    // DIO1 (interrupt)
config.loraRst = 12;    // Reset
config.loraIo1 = 13;    // BUSY pin (NOT DIO1!)
config.freq = 915.0;    // Frequency in MHz
config.bw = 125.0;      // Bandwidth 125 kHz
config.sf = 7;          // Spreading Factor 7
config.syncWord = 0x12; // Private sync word
config.spi = &customSPI; // Custom SPI instance

// Vext power control
pinMode(Vext, OUTPUT);
digitalWrite(Vext, LOW); // Enable power to peripherals
```

## 🐛 Known Issues & Solutions

### Error -705 (ERR_INVALID_FREQUENCY_RANGE)
**Solution**: Ensure `loraIo1 = 13` (BUSY pin), not GPIO 14.

### Display Not Working
**Solution**: Set GPIO 36 (Vext) to LOW before display initialization.

### Nodes Not Finding Each Other
- Verify same frequency (915 MHz)
- Verify same sync word (0x12)
- Check antenna is connected
- Wait 30-60 seconds for initial discovery

## 🌐 Mesh Network Behavior

### Routing Table Timeout
- **Hello Packets**: Sent every 120 seconds (2 minutes)
- **Node Timeout**: 600 seconds (10 minutes)
- Nodes are removed from routing table after 10 minutes of no contact

This is intentional to handle temporary disconnections and network stability.

### For Testing: Faster Timeouts
To modify timeout values for development/testing, edit `src/BuildOptions.h`:
```cpp
// Faster values for testing
#define HELLO_PACKETS_DELAY 30     // 30 seconds
#define DEFAULT_TIMEOUT HELLO_PACKETS_DELAY*3  // 90 seconds
```

## 📊 Testing Results

### Two-Node Test (Verified)
```
Node 1 (0x6674): TX: #15, RX: #12, Nodes: 1
Node 2 (0x8154): TX: #12, RX: #15, Nodes: 1
```
✅ Both nodes communicate successfully
✅ Routing tables updated correctly
✅ Display shows real-time status

### Multi-Node Scaling
- **3 Nodes**: Each shows `Nodes: 2`
- **4 Nodes**: Each shows `Nodes: 3`
- **5 Nodes**: Each shows `Nodes: 4`

All nodes automatically discover each other within 30-60 seconds.

## 🔗 Integration with Main Library

This development folder is **separate** from the main LoRaMesher library:

- **Main Library** (`/src`): Clean, untouched LoRaMesher implementation
- **Heltec Dev** (`/heltec-v3-dev`): Board-specific implementation and testing

The HeltecV3-Display example uses the main library via:
```ini
lib_deps = 
    symlink://../../    # Links to parent LoRaMesher library
    RadioLib
    Adafruit GFX Library
    Adafruit SSD1306
    Adafruit BusIO
```

## 🚦 Development Status

| Feature | Status |
|---------|--------|
| LoRa Radio (SX1262) | ✅ Working |
| OLED Display | ✅ Working |
| Mesh Networking | ✅ Working |
| Multi-node Communication | ✅ Tested |
| Distance-vector Routing | ✅ Working |
| Automatic Node Discovery | ✅ Working |
| Display Layout | ✅ Fixed |
| Boot Screen | ✅ Fixed |

## 📝 Next Steps

- [ ] Test with 5+ nodes
- [ ] Range testing
- [ ] Power consumption optimization
- [ ] Custom timeout configuration for testing
- [ ] Add battery monitoring display
- [ ] Implement sleep modes

## 📄 License

This implementation follows the same license as the main LoRaMesher library.

## 🙏 Credits

- **LoRaMesher Library**: Original mesh networking implementation
- **RadioLib**: LoRa radio communication library
- **Adafruit Libraries**: Display drivers (GFX, SSD1306)
- **Heltec Automation**: Hardware and reference implementations

---

**Branch**: `Heltec-V3`  
**Last Updated**: October 2025  
**Status**: ✅ Production Ready
