# Heltec LoRa 32 V3 Examples

This directory contains examples specifically designed for the **Heltec LoRa 32 V3** board.

## Available Examples

### 1. HeltecV3 - Basic Counter Example

**Path**: `examples/HeltecV3/`

**Description**: A minimal example demonstrating basic LoRaMesher functionality on the Heltec V3.

**Features**:
- ✅ Sends counter packets every 20 seconds
- ✅ Receives and displays packets from other nodes
- ✅ LED indication for TX/RX
- ✅ Serial output for debugging
- ✅ Minimal dependencies

**Best for**:
- First-time setup and testing
- Learning the basics
- Range testing
- Minimal resource usage
- Quick prototyping

**What you'll see**:
```
Initializing LoRaMesher for Heltec LoRa 32 V3...
LoRaMesher initialized successfully!
Local address: 0xABCD
Sending packet #0
Sending packet #1
Packet arrived from 0x1234 with size 4
Hello Counter received nº 42
```

**Quick start**:
```bash
cd examples/HeltecV3
pio run -t upload -t monitor
```

---

### 2. HeltecV3-Display - Counter with OLED Display

**Path**: `examples/HeltecV3-Display/`

**Description**: Enhanced example with full OLED display integration.

**Features**:
- ✅ All features from basic example
- ✅ Real-time display on built-in OLED
- ✅ Shows local address
- ✅ Displays TX/RX status
- ✅ Shows routing table size
- ✅ Scrolling text for long messages
- ✅ Visual feedback without serial monitor

**Best for**:
- Demonstrations and presentations
- Standalone operation (no computer needed)
- Network visualization
- Field deployment
- User-friendly interface

**What you'll see on display**:
```
┌──────────────────────┐
│ Addr: 0xABCD         │
│ TX: Counter #5       │
│ From: 0x1234         │
│ RX: Counter #42      │
│ Routes: 3            │
└──────────────────────┘
```

**Quick start**:
```bash
cd examples/HeltecV3-Display
pio run -t upload -t monitor
```

---

## Comparison Table

| Feature | HeltecV3 (Basic) | HeltecV3-Display |
|---------|------------------|------------------|
| Counter TX/RX | ✅ | ✅ |
| LED indication | ✅ | ✅ |
| Serial output | ✅ | ✅ |
| OLED display | ❌ | ✅ |
| Memory usage | Low | Medium |
| Power usage | Low | Medium+ |
| Dependencies | Minimal | +Display libs |
| Best for | Testing | Production |

## Which Example Should I Use?

### Choose **HeltecV3** (Basic) if:
- 🎯 You're new to LoRaMesher
- 🎯 You want to test hardware first
- 🎯 You're debugging issues
- 🎯 You want minimal power consumption
- 🎯 You prefer serial monitor output
- 🎯 You're doing range testing

### Choose **HeltecV3-Display** if:
- 🎯 You want visual feedback without computer
- 🎯 You're doing demonstrations
- 🎯 You want to monitor network status visually
- 🎯 You're deploying in the field
- 🎯 You want a more complete example

## Common to Both Examples

### Pin Configuration
```cpp
LORA_CS     = 8   // SPI Chip Select
LORA_IRQ    = 14  // LoRa interrupt
LORA_RST    = 12  // Reset pin
LORA_IO1    = 14  // DIO1 pin
LORA_MOSI   = 10  // SPI MOSI
LORA_MISO   = 11  // SPI MISO
LORA_SCK    = 9   // SPI Clock
LED         = 35  // Built-in LED
```

### Default LoRa Parameters
```cpp
Frequency: 868.0 MHz  // Change for your region!
Bandwidth: 125.0 kHz
Spreading Factor: 7
Coding Rate: 7 (4/7)
TX Power: 14 dBm
Sync Word: 0x12
```

### Important Notes
⚠️ **Always connect antenna before powering on!**  
⚠️ **Adjust frequency for your region** (868 MHz EU, 915 MHz US, 433 MHz Asia)  
⚠️ **Match LoRa parameters across all nodes in your network**

## Getting Started

### 1. Hardware Preparation
- Connect antenna to Heltec V3
- Connect board to computer via USB-C

### 2. Choose Your Example
- Start with **HeltecV3** for testing
- Upgrade to **HeltecV3-Display** when ready

### 3. Open in PlatformIO
```bash
# Option 1: Open in VS Code
code examples/HeltecV3

# Option 2: Command line
cd examples/HeltecV3
pio run -t upload -t monitor
```

### 4. Customize (Optional)
Edit `src/main.cpp` to:
- Change frequency (`config.freq = 915.0;`)
- Adjust TX power (`config.power = 14;`)
- Modify transmission interval
- Change packet structure

### 5. Multi-Node Testing
- Upload to multiple boards
- They will automatically form a mesh network
- Check serial output or display to see routing

## Modifying Examples

### Change Frequency
```cpp
// In setupLoraMesher() function
config.freq = 915.0;  // Change to your frequency
```

### Change Transmission Interval
```cpp
// In loop() function
vTaskDelay(20000 / portTICK_PERIOD_MS);  // 20 seconds
// Change to:
vTaskDelay(10000 / portTICK_PERIOD_MS);  // 10 seconds
```

### Change TX Power
```cpp
// In setupLoraMesher() function
config.power = 22;  // Max power (careful with regulations!)
```

### Add Custom Data
```cpp
// Define your structure
struct myData {
    uint32_t value1;
    float value2;
    char message[20];
};

// Use instead of dataPacket
myData* packet = new myData;
packet->value1 = 123;
packet->value2 = 45.67;
strcpy(packet->message, "Hello");
```

## Troubleshooting

### Example Won't Compile
1. Check PlatformIO is properly installed
2. Let PlatformIO install all dependencies
3. Check for typos in `platformio.ini`
4. Try PlatformIO: Clean

### Board Not Detected
1. Install CP210x USB drivers
2. Check USB cable (must be data cable)
3. Try different USB port
4. Hold PRG button while connecting

### No LoRa Communication
1. Verify antenna is connected
2. Check frequency is correct for region
3. Ensure all nodes use same LoRa parameters
4. Test at close range first (1-2 meters)
5. Check serial output for errors

### Display Not Working (Display example)
1. Verify I2C pins (SDA=17, SCL=18)
2. Check display libraries are installed
3. Try basic example first to verify LoRa works
4. Check for I2C address conflicts

## Next Steps

After testing the examples:

1. **Read the Documentation**:
   - [Complete Guide](../HELTEC_V3_GUIDE.md)
   - [Quick Reference](../HELTEC_V3_QUICK_REF.md)
   - [Migration Guide](../MIGRATION_TO_HELTEC_V3.md)

2. **Customize for Your Project**:
   - Modify packet structure
   - Add sensors
   - Implement sleep modes
   - Add WiFi connectivity

3. **Deploy Network**:
   - Test range in your environment
   - Optimize parameters
   - Monitor performance
   - Scale up node count

4. **Get Involved**:
   - Share your results
   - Join [Discord](https://discord.gg/SSaZhsChjQ)
   - Contribute improvements
   - Report issues

## Additional Examples

Looking for more? Check the main examples directory for:
- **Counter** - Generic counter example (other boards)
- **CounterAndDisplay** - Generic display example
- **LargePayload** - Handling large data transfers
- **SX1262** - Generic SX1262 example

These may need modification for Heltec V3 pins, but can provide inspiration for your projects.

## Resources

### Hardware
- [Heltec V3 Official Page](https://heltec.org/project/wifi-lora-32-v3/)
- [Board Pinout](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3.png)

### Software
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/)
- [LoRaMesher GitHub](https://github.com/LoRaMesher/LoRaMesher)
- [PlatformIO Documentation](https://docs.platformio.org/)

### Community
- [Discord Server](https://discord.gg/SSaZhsChjQ)
- [Heltec Forum](https://heltec.org/community/)

---

**Need help?** Check the [Setup Checklist](../HELTEC_V3_SETUP_CHECKLIST.md) or ask on Discord!

**Happy Meshing! 🚀**
