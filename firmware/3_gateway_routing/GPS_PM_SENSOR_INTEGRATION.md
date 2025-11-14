# GPS + PM Sensor Integration Guide

**Branch:** feature/gps-pm-integration-sensor
**Date:** November 14, 2025
**Sensors:** PMS7003 (Particulate Matter) + NEO-M8M-0-10 (GPS)

---

## Hardware Configuration

### Sensor Wiring (Your Setup)

**PMS7003 Particulate Matter Sensor:**
```
PMS7003 Pin → Heltec V3 GPIO → Extended Board
Pin 9 (TX)  → GPIO 7 (RX)    → Connected
Pin 7 (RX)  → GPIO 6 (TX)    → Connected
VCC (5V)    → 5V             → Extended board power
GND         → GND            → Extended board ground
```

**NEO-M8M-0-10 GPS Module:**
```
GPS Pin     → Heltec V3 GPIO → Extended Board
TX          → GPIO 5 (RX)    → Connected
RX          → GPIO 4 (TX)    → Connected
VCC (3.3V)  → 3.3V           → Extended board power
GND         → GND            → Extended board ground
```

**Pin Verification:**
- ✅ GPIO 4, 5, 6, 7: All FREE (no conflicts with LoRa, OLED, or control pins)
- ✅ PMS7003: Uses 5V power, 3.3V logic (ESP32-S3 compatible)
- ✅ GPS: Uses 3.3V power and logic (native ESP32-S3)
- ✅ Extended board provides power distribution for both sensors

---

## Software Architecture

### New Files Created

1. **`src/sensor_data.h`** - Enhanced data packet structure (26 bytes)
   - PM data: PM1.0, PM2.5, PM10 (6 bytes)
   - GPS data: Lat, Lon, Alt, Satellites (14 bytes)
   - Metadata: Timestamp, Sequence (6 bytes)
   - Helper functions: createPacket(), validate(), getAQICategory()

2. **`src/pms7003_parser.h`** - PMS7003 UART parser
   - Reads 32-byte frames from sensor
   - Validates checksums
   - Extracts PM1.0, PM2.5, PM10 concentrations
   - Auto-updates in background

3. **`src/gps_handler.h`** - GPS NMEA parser using TinyGPS++
   - Parses NMEA sentences ($GPGGA, $GPRMC)
   - Extracts Lat, Lon, Alt, Satellites
   - Validates GPS fix quality
   - Tracks fix age

### Modified Files

1. **`src/heltec_v3_pins.h`** - Added sensor pin definitions
   ```cpp
   #define PMS_RX_PIN  7
   #define PMS_TX_PIN  6
   #define GPS_RX_PIN  5
   #define GPS_TX_PIN  4
   ```

2. **`platformio.ini`** - Added TinyGPS++ library
   ```ini
   lib_deps =
       mikalhart/TinyGPSPlus @ ^1.0.3
   ```

3. **`src/main.cpp`** - Integrated sensor reading and data transmission
   - Added `HardwareSerial` objects for PM and GPS
   - Created `sensorReadingTask()` for continuous sensor updates
   - Modified `sendSensorData()` to pack PM + GPS data
   - Modified `processReceivedPackets()` to parse enhanced packets at gateway

---

## Data Flow

### Sensor Node (Node 1 or 2):

```
PMS7003 Sensor (UART1, 9600 baud)
    ↓ Continuous reading (every ~1 second)
PM Parser (Background Task)
    ↓ Store latest: PM1.0, PM2.5, PM10

NEO-M8M GPS (UART2, 9600 baud)
    ↓ NMEA sentences (every ~1 second)
GPS Handler (Background Task)
    ↓ Store latest: Lat, Lon, Alt, Sats

Every 60 seconds:
    ↓ Read latest PM + GPS data
Enhanced Packet Creator
    ↓ Pack into 26-byte structure
LoRa Transmission
    ↓ Send to gateway via mesh
```

### Gateway Node (Node 5):

```
LoRa Reception
    ↓ Receive enhanced packet
Packet Parser
    ↓ Extract PM and GPS data
Validation
    ↓ Check ranges, GPS fix validity
Display + Serial Output
    ↓ Show PM2.5, GPS coordinates, AQI category
MQTT/Database (Future)
```

---

## Packet Structure Details

### EnhancedSensorData (26 bytes total):

```cpp
struct EnhancedSensorData {
    uint16_t pm1_0;      // Bytes 0-1: PM1.0 (µg/m³)
    uint16_t pm2_5;      // Bytes 2-3: PM2.5 (µg/m³)
    uint16_t pm10;       // Bytes 4-5: PM10 (µg/m³)

    float latitude;      // Bytes 6-9: Decimal degrees
    float longitude;     // Bytes 10-13: Decimal degrees
    float altitude;      // Bytes 14-17: Meters

    uint8_t satellites;  // Byte 18: Satellite count
    uint8_t gps_valid;   // Byte 19: Fix status (0/1)

    uint32_t timestamp;  // Bytes 20-23: Milliseconds
    uint16_t sequence;   // Bytes 24-25: Packet number
};
```

**LoRa Packet Overhead:**
- LoRaMesher header: ~8-12 bytes
- Enhanced payload: 26 bytes
- **Total: ~34-38 bytes**

**Time-on-Air (SF7, BW125, CR4/5):**
- ~38 bytes ≈ 70 ms (updated in firmware)
- 60-second interval = 70ms/60000ms = 0.12% duty cycle per node
- **Well within 1% regulatory limit** ✅

---

## Expected Serial Output

### Sensor Node (During Operation):

```
=================================
xMESH GATEWAY-AWARE COST ROUTING
Role: SENSOR (SENSOR)
=================================

--- Initializing Sensors ---
[PMS] Initialized on RX=7, TX=6, baud=9600
[PMS] Ready
✅ PMS7003 PM sensor initialized

[GPS] Initialized on RX=5, TX=4, baud=9600
[GPS] Waiting for satellite fix...
[GPS] Note: May take 1-5 minutes outdoors, longer indoors
✅ NEO-M8M GPS module initialized
--- Sensors Ready ---

✅ Sensor reading task created

[SENSOR_TASK] Started
[PMS] PM1.0=5 PM2.5=12 PM10=18 µg/m³ (age=850ms)
[GPS] 13.732500°N, 100.547800°E, 8 sats, alt=15.2m (age=1200ms)

TX: Seq=0 to Gateway=0005 (Hops=1)
  PM: 1.0=5 2.5=12 10=18 µg/m³
  GPS: 13.732500°N, 100.547800°E, 8 sats
```

### Gateway Node (Receiving Packets):

```
RX: Seq=0 From=0001
  PM: 1.0=5 2.5=12 10=18 µg/m³ (AQI: Good)
  GPS: 13.732500°N, 100.547800°E, alt=15.2m, 8 sats (Excellent)
Link quality: SNR=9 dB, Est.RSSI=-93 dBm

[GATEWAY] Packet 0 from 0001 received
  ✓ Packet validation passed
```

---

## Testing Procedure

### Phase 1: Sensor Hardware Testing (30 minutes)

**Test PM Sensor:**
```bash
cd firmware/3_gateway_routing
./flash_node.sh 1 /dev/cu.usbserial-0001
pio device monitor --port /dev/cu.usbserial-0001 --baud 115200
```

**Expected Output:**
- `[PMS] Ready` within 2 seconds
- `[PMS] PM1.0=X PM2.5=Y PM10=Z` every 60 seconds
- PM values should update (not always 0)

**Test GPS:**
- Move sensor node near window/outdoors
- Wait 1-5 minutes for satellite acquisition
- **Expected:** `[GPS] Lat: X.XXXXXX°, Lon: Y.YYYYYY°, Sats: 4-12`
- GPS valid should become `1` after fix acquired

**Troubleshooting:**
- PM always 0: Check wiring, 5V power, RX/TX not swapped
- GPS no fix: Move outdoors, wait longer (cold start can take 5min)
- Checksum errors: Check baud rate (9600), RX/TX pin assignments

---

### Phase 2: LoRa Transmission Test (30 minutes)

**Setup:**
```
[Sensor:Node1+PM+GPS] --4m-- [Gateway:Node5]
      Battery/USB              USB Monitor
```

**Flash Sensor:**
```bash
./flash_node.sh 1 /dev/cu.usbserial-0001
```

**Flash Gateway:**
```bash
./flash_node.sh 5 /dev/cu.usbserial-5
```

**Monitor Gateway:**
```bash
pio device monitor --port /dev/cu.usbserial-5 --baud 115200
```

**Success Criteria:**
- [ ] Gateway shows: `RX: Seq=X From=0001`
- [ ] Gateway shows: `PM: 1.0=X 2.5=Y 10=Z µg/m³`
- [ ] Gateway shows: `GPS: XX.XXXXXX°N, YY.YYYYYY°E` (if fix acquired)
- [ ] PDR: 100% (all packets received)
- [ ] No UART errors or packet corruption

---

### Phase 3: Multi-Hop Test (Optional, 30 minutes)

**Setup:**
```
[Sensor:Node1+PM+GPS] --8m-- [Relay:Node3] --8m-- [Gateway:Node5]
```

**Test with reduced power (6 dBm):**
- Edit `heltec_v3_pins.h`: `DEFAULT_LORA_TX_POWER = 6`
- Clean build + flash all nodes
- Monitor gateway

**Verify:**
- Gateway routing table shows `hops>0`
- PM + GPS data survives multi-hop forwarding
- PDR remains >95%

---

## Power Requirements

**PMS7003:**
- Standby: ~10 mA
- Active (fan running): ~80-100 mA @ 5V
- **Recommendation:** USB power or large battery (2000+ mAh)

**NEO-M8M GPS:**
- Acquisition: ~30 mA
- Tracking: ~20 mA @ 3.3V
- **Recommendation:** USB power or separate battery

**Combined:**
- Peak: ~120-130 mA (both sensors active)
- Average: ~100 mA (continuous operation)
- **Battery life:** 2000 mAh battery ≈ 20 hours with both sensors

**For Field Tests:**
- Option 1: USB power (recommended for stationary tests)
- Option 2: 10,000 mAh power bank (mobile tests)
- Option 3: Disable PM fan between readings (reduce to ~30 mA total)

---

## Known Limitations & Future Work

### Current Implementation:

✅ **Implemented:**
- PMS7003 reading via UART (9600 baud)
- GPS reading via TinyGPS++ (NMEA parsing)
- Enhanced 26-byte packet structure
- LoRa transmission of PM + GPS data
- Gateway reception and parsing
- AQI category calculation
- GPS quality indicators

⚠️ **Limitations:**
1. **GPS Fix Time:** 1-5 minutes cold start (requires outdoor/window)
2. **Indoor GPS:** May not acquire fix indoors (expected limitation)
3. **Power Consumption:** ~100 mA average (requires USB or large battery)
4. **PM Sensor Warmup:** ~30 seconds for stable readings
5. **No Time Sync:** GPS time available but not used for timestamp yet

🔄 **Future Enhancements:**
1. PM sensor sleep mode (reduce power between readings)
2. GPS time synchronization across nodes
3. Kalman filtering for GPS coordinates
4. PM data averaging (reduce noise)
5. MQTT publishing at gateway
6. Database storage
7. Web dashboard for visualization

---

## Code References

### Sensor Initialization:
- [main.cpp:1886-1905](../src/main.cpp#L1886-L1905) - Sensor setup in `setup()`

### Sensor Reading Task:
- [main.cpp:1706-1738](../src/main.cpp#L1706-L1738) - Background UART reading

### Sensor Data Transmission:
- [main.cpp:1746-1830](../src/main.cpp#L1746-L1830) - Pack and send enhanced data

### Gateway Reception:
- [main.cpp:1500-1573](../src/main.cpp#L1500-L1573) - Parse and display enhanced packets

### Data Structures:
- [sensor_data.h](../src/sensor_data.h) - EnhancedSensorData structure
- [pms7003_parser.h](../src/pms7003_parser.h) - PM sensor parser
- [gps_handler.h](../src/gps_handler.h) - GPS NMEA handler

### Pin Definitions:
- [heltec_v3_pins.h:34-42](../src/heltec_v3_pins.h#L34-L42) - Sensor GPIO assignments

---

## Compilation Results

**Build Status:** ✅ SUCCESS
**RAM Usage:** 6.8% (22,432 / 327,680 bytes)
**Flash Usage:** 32.5% (425,525 / 1,310,720 bytes)
**Build Time:** 14.72 seconds

**Memory Impact:**
- Before (Protocol 3 only): ~400 KB flash
- After (Protocol 3 + sensors): ~425 KB flash
- **Increase:** ~25 KB (PMS parser + GPS handler + TinyGPS++)
- **Remaining:** 67.5% flash available for future features ✅

---

## Validation Checklist

### Hardware Validation:
- [ ] PMS7003 sensor physically connected to GPIO 6, 7
- [ ] GPS module physically connected to GPIO 4, 5
- [ ] Extended board provides 5V (PM) and 3.3V (GPS)
- [ ] All GND connections common

### Firmware Validation:
- [x] Firmware compiles without errors
- [x] Pin definitions match wiring
- [x] UART baud rates correct (9600)
- [x] Packet structure validated
- [ ] Flash to sensor node successful
- [ ] Serial monitor shows sensor readings
- [ ] PM values update (not stuck at 0)
- [ ] GPS acquires fix (outdoors/window)

### LoRa Mesh Validation:
- [ ] Gateway receives enhanced packets
- [ ] PM data parses correctly at gateway
- [ ] GPS data parses correctly at gateway
- [ ] PDR remains >95% with larger packets
- [ ] Duty cycle remains <1%
- [ ] Multi-hop works with enhanced packets

---

## Next Steps

### Immediate Testing (Today):
1. Flash sensor node (Node 1) with GPS + PM integration
2. Flash gateway node (Node 5)
3. Verify serial output shows PM and GPS readings
4. Test LoRa transmission and gateway reception
5. Validate packet integrity and PDR

### Extended Testing (After Initial Validation):
1. Multi-hop test with relay (3 nodes)
2. Long-duration test (1-2 hours)
3. Power consumption measurement
4. GPS accuracy validation (compare with Google Maps)
5. PM sensor calibration (compare with reference sensor)

### Documentation:
1. Update README.md with sensor integration
2. Add wiring diagrams
3. Document testing results
4. Create user guide for sensor deployment

---

## Troubleshooting Guide

### Problem: PM sensor always shows 0

**Possible Causes:**
1. ❌ No 5V power (fan doesn't run)
2. ❌ RX/TX pins swapped
3. ❌ Wrong baud rate
4. ❌ Sensor not powered on

**Solutions:**
- Check 5V rail with multimeter
- Swap GPIO 6 and 7 definitions
- Verify PMS_BAUD = 9600
- Check PMS7003 LED (should be on)

---

### Problem: GPS never gets fix

**Possible Causes:**
1. ❌ Indoors (no satellite visibility)
2. ❌ Cold start (needs time)
3. ❌ Antenna not connected
4. ❌ RX/TX pins swapped

**Solutions:**
- Move to window or outdoors
- Wait 5-10 minutes for cold start
- Check GPS module has antenna
- Swap GPIO 4 and 5 if needed
- Check serial output for NMEA sentences

---

### Problem: Gateway receives corrupted data

**Possible Causes:**
1. ❌ Packet size mismatch (old SensorData vs new EnhancedSensorData)
2. ❌ Struct padding issues
3. ❌ Gateway using old firmware

**Solutions:**
- Verify both sensor and gateway use EnhancedSensorData
- Check `sizeof(EnhancedSensorData) == 26`
- Reflash gateway with updated firmware
- Check serial logs for parsing errors

---

## Air Quality Index (AQI) Reference

Based on PM2.5 concentration:

| PM2.5 (µg/m³) | AQI Category | Health Impact |
|---------------|--------------|---------------|
| 0-12 | Good | Minimal |
| 13-35 | Moderate | Acceptable |
| 36-55 | Unhealthy (Sensitive) | Sensitive groups may experience effects |
| 56-150 | Unhealthy | Everyone may experience effects |
| 151-250 | Very Unhealthy | Health alert |
| 251+ | Hazardous | Emergency conditions |

---

## GPS Fix Quality

Based on satellite count:

| Satellites | Quality | HDOP Typical | Accuracy |
|------------|---------|--------------|----------|
| 0-3 | No Fix | >10 | Invalid |
| 4-5 | Poor | 5-10 | ±50-100m |
| 6-7 | Fair | 2-5 | ±10-20m |
| 8+ | Good-Excellent | <2 | ±5-10m |

**Note:** GPS accuracy depends on:
- Satellite geometry (HDOP - Horizontal Dilution of Precision)
- Atmospheric conditions
- Multipath interference (buildings, trees)
- Antenna quality

---

**Integration Complete!**
**Next:** Flash to hardware and validate sensor readings → LoRa transmission → Gateway reception
