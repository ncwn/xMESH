# Heltec V3 Quick Start

## ⚡ Flash and Run (5 Minutes)

### 1. Connect Hardware
```bash
# Connect Heltec LoRa 32 V3 via USB-C
# Attach LoRa antenna (MANDATORY!)
```

### 2. Build and Upload
```bash
cd heltec-v3-dev/examples/HeltecV3-Display
pio run -t upload -t monitor
```

### 3. Expected Output

**Serial Monitor:**
```
========================================
Heltec V3 LoRaMesher with Display
========================================

Initializing LoRaMesher for Heltec LoRa 32 V3...
LoRaMesher initialized successfully!
Local address: 0x6674
Sending packet #0
Sending packet #1
...
```

**OLED Display:**
```
0x6674        ← Your node address
TX: #3        ← Transmission counter
              ← Empty (no RX yet)
Nodes: 0      ← No other nodes detected
```

## 🌐 Two-Node Test

### Flash Second Board
1. Connect second Heltec V3 board
2. Run same command: `pio run -t upload`
3. Power on both boards

### Expected After ~30 Seconds

**Board 1 Display:**
```
0x6674
TX: #5
RX: #3
Nodes: 1      ← Detected 1 other node!
```

**Board 2 Display:**
```
0x8154
TX: #3
RX: #5
Nodes: 1      ← Detected 1 other node!
```

✅ **Mesh network is working!**

## 📊 What's Happening?

1. **TX Counter** - Increments every 20 seconds when broadcasting
2. **RX Counter** - Shows last received packet counter from other nodes
3. **Nodes Count** - Number of other nodes in routing table
4. **Auto Discovery** - Nodes find each other automatically (30-60s)

## 🔧 Pin Configuration Reference

```cpp
// LoRa SX1262
#define LORA_CS     8   // NSS/CS
#define LORA_IRQ    14  // DIO1 interrupt
#define LORA_RST    12  // Reset
#define LORA_IO1    13  // BUSY pin (critical!)
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9

// OLED Display
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21

// Power & LED
#define Vext        36  // Must be LOW to power peripherals
#define BOARD_LED   35
```

## ⚠️ Common Issues

### Display Not Working
```cpp
// Ensure Vext is enabled in setup()
pinMode(Vext, OUTPUT);
digitalWrite(Vext, LOW);  // LOW = ON
```

### Error -705 (Radio Not Working)
```cpp
// Ensure BUSY pin is correctly set
config.loraIo1 = 13;  // BUSY pin, NOT DIO1!
```

### Nodes Not Finding Each Other
- ✅ Same frequency (915 MHz)
- ✅ Same sync word (0x12)
- ✅ Antenna connected
- ✅ Wait 30-60 seconds
- ✅ Within LoRa range

## 📝 Next Steps

- ✅ **Working?** Add more nodes (3-5 boards)
- 📚 **Learn More**: See [README.md](README.md) for full documentation
- 🔧 **Customize**: Edit `src/main.cpp` for your application
- 🐛 **Issues?**: Check [docs/HELTEC_V3_GUIDE.md](docs/HELTEC_V3_GUIDE.md)

## 🎯 Development Workflow

```bash
# Edit code
nano examples/HeltecV3-Display/src/main.cpp

# Build and upload
cd examples/HeltecV3-Display
pio run -t upload

# Monitor serial output
pio device monitor
```

## 📈 Scaling Test

| Nodes | Each Shows | Network Size |
|-------|------------|--------------|
| 2     | Nodes: 1   | Small        |
| 3     | Nodes: 2   | Medium       |
| 5     | Nodes: 4   | Good         |
| 10    | Nodes: 9   | Large        |

---

**Ready in 5 minutes! 🚀**
