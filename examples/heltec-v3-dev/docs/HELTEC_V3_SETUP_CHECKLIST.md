# Heltec LoRa 32 V3 - Setup Checklist

Use this checklist to verify your Heltec LoRa 32 V3 setup is correct.

## ☑️ Pre-Setup Checklist

### Hardware
- [ ] Heltec LoRa 32 V3 board received
- [ ] Appropriate antenna for your frequency band (863-928 MHz)
- [ ] USB-C cable (data cable, not charge-only)
- [ ] Computer with USB port

### Software
- [ ] Visual Studio Code installed
- [ ] PlatformIO extension installed in VS Code
- [ ] CP210x USB driver installed (if needed)

## ☑️ Hardware Setup Checklist

### Physical Connections
- [ ] **CRITICAL**: Antenna connected to the board
  - ⚠️ NEVER power on without antenna - can damage RF amplifier!
- [ ] USB-C cable connected to board
- [ ] USB cable connected to computer
- [ ] Board powers on (small power LED lights up)

### Verification
- [ ] Board is detected by computer
  - macOS: Check in "System Information" → USB
  - Windows: Check in "Device Manager" → Ports
  - Linux: Run `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`

## ☑️ Software Setup Checklist

### Project Setup
- [ ] Cloned/downloaded the Heltec-V3 branch
- [ ] Opened example project in VS Code (`examples/HeltecV3` or `examples/HeltecV3-Display`)
- [ ] PlatformIO recognizes the project (shows in PlatformIO tab)

### Configuration Verification
- [ ] `platformio.ini` has correct board: `heltec_wifi_lora_32_V3`
- [ ] Frequency set correctly for your region (in `main.cpp`):
  - [ ] 868.0 MHz for Europe
  - [ ] 915.0 MHz for North America
  - [ ] 433.0 MHz for Asia
- [ ] All pin definitions match:
  ```cpp
  LORA_CS   = 8
  LORA_IRQ  = 14
  LORA_RST  = 12
  LORA_IO1  = 14
  LORA_MOSI = 10
  LORA_MISO = 11
  LORA_SCK  = 9
  ```

## ☑️ First Build Checklist

### Building
- [ ] Click "Build" in PlatformIO (checkmark icon)
- [ ] Build completes without errors
- [ ] If build fails, check:
  - [ ] All dependencies installed (RadioLib, Adafruit libraries if using display)
  - [ ] No syntax errors in code
  - [ ] Correct framework selected (Arduino)

### Uploading
- [ ] Select correct serial port in PlatformIO
- [ ] Click "Upload" (arrow icon)
- [ ] If upload fails:
  - [ ] Try holding "PRG" button while uploading
  - [ ] Check USB cable is data cable
  - [ ] Verify drivers are installed
  - [ ] Try different USB port

### First Run
- [ ] Upload completes successfully
- [ ] Open Serial Monitor (115200 baud)
- [ ] See initialization messages:
  - [ ] "Initializing LoRaMesher for Heltec LoRa 32 V3..."
  - [ ] "LoRaMesher initialized successfully!"
  - [ ] Local address displayed (e.g., "Local address: 0xABCD")
- [ ] LED flashes on boot (2 quick flashes)
- [ ] If using display example:
  - [ ] OLED screen turns on
  - [ ] Text appears on display

## ☑️ Functionality Checklist

### Basic Operation
- [ ] Board sends packets periodically (check serial output)
- [ ] LED blinks when sending
- [ ] Packet counter increments
- [ ] No error messages in serial monitor

### Display (if using HeltecV3-Display)
- [ ] Display shows local address
- [ ] Display updates with TX counter
- [ ] Display shows routing table info
- [ ] Text scrolls if too long

### Two-Board Testing (if available)
- [ ] Both boards use same configuration:
  - [ ] Same frequency
  - [ ] Same spreading factor (SF)
  - [ ] Same bandwidth (BW)
  - [ ] Same coding rate (CR)
  - [ ] Same sync word
- [ ] Board 1 receives packets from Board 2
- [ ] Board 2 receives packets from Board 1
- [ ] LED blinks on both boards when receiving
- [ ] Serial monitor shows received packets on both
- [ ] Routing table builds on both boards

## ☑️ Performance Checklist

### Signal Quality
- [ ] Monitor RSSI values (should be > -120 dBm for good link)
- [ ] Monitor SNR values (should be > 0 dB for good link)
- [ ] Check for packet loss (compare sent vs received)

### Range Testing (optional)
- [ ] Test at close range (1-2 meters) - should work perfectly
- [ ] Test at medium range (10-50 meters)
- [ ] Test at longer range (up to several km in open areas)
- [ ] Note performance at different spreading factors

## ☑️ Troubleshooting Checklist

### If No Communication
- [ ] Verify antenna is connected
- [ ] Check frequency matches region and hardware
- [ ] Ensure both nodes have matching LoRa parameters
- [ ] Verify SPI pins are correct
- [ ] Check for error messages in serial output
- [ ] Try moving boards closer together
- [ ] Reduce spreading factor (try SF7)

### If Display Not Working
- [ ] Verify I2C address (0x3C)
- [ ] Check SDA (17) and SCL (18) pin definitions
- [ ] Confirm display libraries installed
- [ ] Try I2C scanner sketch to detect display
- [ ] Check OLED_RST pin (21)

### If Upload Fails
- [ ] Install/update CP210x drivers
- [ ] Hold "PRG" button during upload
- [ ] Try different USB cable
- [ ] Check serial port permissions (Linux/macOS)
- [ ] Reduce upload speed to 115200

### If High Power Consumption
- [ ] Control VEXT pin (GPIO 36) to power off display
- [ ] Implement sleep modes
- [ ] Reduce TX power if range permits
- [ ] Increase time between transmissions

## ☑️ Final Verification

### System Ready When:
- [ ] ✅ Board uploads successfully
- [ ] ✅ Serial monitor shows initialization
- [ ] ✅ Packets are being sent
- [ ] ✅ LED blinks appropriately
- [ ] ✅ Display works (if using display example)
- [ ] ✅ Can communicate with other nodes (if multi-node)
- [ ] ✅ No error messages

## 📋 Configuration Summary

Record your working configuration:

```
Board: Heltec LoRa 32 V3
Frequency: _________ MHz
Spreading Factor: SF_____
Bandwidth: _________ kHz
Coding Rate: 4/_____
TX Power: _________ dBm
Local Address: 0x_____________
Date Tested: _________________
```

## 🆘 Getting Help

If you've gone through this checklist and still have issues:

1. **Check Documentation**:
   - [Complete Guide](HELTEC_V3_GUIDE.md)
   - [Quick Reference](HELTEC_V3_QUICK_REF.md)
   - [Migration Guide](MIGRATION_TO_HELTEC_V3.md)

2. **Community Support**:
   - Join [Discord](https://discord.gg/SSaZhsChjQ)
   - Search GitHub issues
   - Post detailed question with:
     - Checklist status
     - Error messages
     - Serial output
     - Configuration used

3. **Debug Mode**:
   ```cpp
   // Add to setup() for verbose logging
   Serial.setDebugOutput(true);
   ```

## ✅ Success Criteria

Your setup is successful when:
- All items in "Final Verification" are checked ✅
- Board operates reliably for extended period
- Communication range meets expectations
- No intermittent issues or crashes

---

**Version**: 1.0  
**Last Updated**: October 2025  
**Branch**: Heltec-V3

🎉 **Congratulations on completing your Heltec LoRa 32 V3 setup!**
