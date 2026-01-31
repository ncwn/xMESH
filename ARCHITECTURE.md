# xMESH Architecture Documentation

**Version:** 1.0  
**Last Updated:** 2025-01-31  
**Audience:** Developers working on or with xMESH

## Overview

xMESH is a production-ready LoRa mesh networking stack for ESP32, built as an extension of the [LoRaMesher](https://github.com/LoRaMesher/LoRaMesher) library. It adds advanced multi-metric routing, AES encryption, adaptive scheduling, and IoT sensor integration for environmental monitoring applications.

**Target Hardware:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)  
**Radio:** LoRa 923MHz (AS923 band), SF7, BW125kHz  
**Use Case:** Air quality sensor mesh with gateway-to-cloud forwarding

## High-Level Architecture

```
+-----------------------------------------------------------------------+
|                         APPLICATION LAYER                              |
|  +------------------+  +------------------+  +------------------+      |
|  | Production Main  |  | Serial Console   |  | MQTT Gateway     |      |
|  | (main.cpp)       |  | (commands)       |  | (PubSubClient)   |      |
|  +------------------+  +------------------+  +------------------+      |
+-----------------------------------------------------------------------+
                                    |
+-----------------------------------------------------------------------+
|                         xMESH EXTENSION LAYER                          |
|  +------------------+  +------------------+  +------------------+      |
|  | xmesh-core       |  | xmesh-security   |  | xmesh-hal        |      |
|  | - CostRouter     |  | - SecurityManager|  | - Sensors        |      |
|  | - ETXTracker     |  | - PayloadCrypto  |  | - Display        |      |
|  | - TrickleScheduler| | - FrameCounter   |  | - SensorPacket   |      |
|  | - GatewayBalancer|  | - KeyManager     |  +------------------+      |
|  | - MobilityDetector| | - DeviceAuth     |  +------------------+      |
|  | - RoutingAdapter |  +------------------+  | xmesh-ota        |      |
|  +------------------+                        | - OTAManager     |      |
+-----------------------------------------------------------------------+
                                    |
+-----------------------------------------------------------------------+
|                    LoRaMesher MESH LAYER (Modified Fork)               |
|  +------------------+  +------------------+  +------------------+      |
|  | LoraMesher       |  | RoutingTableSvc  |  | PacketService    |      |
|  | - Packet TX/RX   |  | - Route selection|  | - Packet creation|      |
|  | - HELLO protocol |  | - Cost callbacks |  | - Serialization  |      |
|  | - Queue mgmt     |  | - Table timeout  |  +------------------+      |
|  +------------------+  +------------------+                            |
+-----------------------------------------------------------------------+
                                    |
+-----------------------------------------------------------------------+
|                         RADIO LAYER                                    |
|  +------------------+  +------------------+                            |
|  | RadioLib         |  | LM_SX1262        |                            |
|  | (SX1262 driver)  |  | (HAL wrapper)    |                            |
|  +------------------+  +------------------+                            |
+-----------------------------------------------------------------------+
```

## Directory Structure

```
xMESH/
├── src/                      # Modified LoRaMesher core
│   ├── LoraMesher.cpp/h      # Main mesh singleton (modified)
│   ├── BuildOptions.h        # Compile options (LM_GOD_MODE added)
│   ├── services/
│   │   ├── RoutingTableService.cpp/h  # Route management (callbacks added)
│   │   ├── PacketService.cpp/h        # Packet creation
│   │   ├── PacketQueueService.cpp/h   # Queue management
│   │   └── RoleService.cpp/h          # Gateway/node roles
│   ├── entities/
│   │   ├── packets/          # Packet type definitions
│   │   └── routingTable/     # RouteNode, NetworkNode
│   └── modules/
│       └── LM_SX1262.cpp/h   # Radio HAL for Heltec V3
│
├── lib/                      # xMESH extension libraries
│   ├── xmesh-core/           # Advanced routing algorithms
│   │   ├── CostRouter.cpp/h          # Multi-metric cost function
│   │   ├── ETXTracker.cpp/h          # Link quality tracking
│   │   ├── TrickleScheduler.cpp/h    # Adaptive HELLO (RFC 6206)
│   │   ├── GatewayBalancer.cpp/h     # Load distribution
│   │   ├── MobilityDetector.cpp/h    # Movement detection
│   │   └── RoutingAdapter.cpp/h      # Thread-safe route access
│   │
│   ├── xmesh-security/       # Encryption & authentication
│   │   ├── SecurityManager.cpp/h     # Facade (use this only)
│   │   ├── PayloadCrypto.cpp/h       # AES-256-GCM
│   │   ├── KeyManager.cpp/h          # Key derivation (PBKDF2)
│   │   ├── FrameCounter.cpp/h        # Replay protection
│   │   └── DeviceAuth.cpp/h          # Device whitelist
│   │
│   ├── xmesh-hal/            # Hardware abstraction
│   │   ├── Sensors.cpp/h             # PMS7003 + GPS
│   │   ├── Display.cpp/h             # SSD1306 OLED
│   │   └── SensorPacket.h            # 23-byte packet format
│   │
│   └── xmesh-ota/            # Over-the-air updates
│       └── OTAManager.cpp/h          # ArduinoOTA + HTTP OTA
│
├── firmware/production/      # Main entry point
│   ├── src/main.cpp          # Application (~1300 lines)
│   ├── include/
│   │   ├── config.h          # All configurable parameters
│   │   └── DutyCycleBudget.h # Regulatory compliance
│   ├── platformio.ini        # Build configuration
│   └── test/test_native/     # Unity test suite
│
├── .LoRaMesher-OG/           # Original LoRaMesher snapshot
├── mqtt-setup/               # Docker Mosquitto config
└── AGENTS.md                 # AI agent knowledge base
```

## Module Deep-Dive

### 1. xmesh-core: Advanced Routing

#### CostRouter - Multi-Metric Route Selection

The heart of xMESH's routing improvement over vanilla LoRaMesher.

**Cost Formula:**
```
cost = W1*hops + W2*(1-norm_RSSI) + W3*(1-norm_SNR) + W4*(ETX-1) + W5*gatewayBias
```

| Weight | Default | Parameter |
|--------|---------|-----------|
| W1 | 1.0 | Hop count (primary) |
| W2 | 0.3 | Signal strength (RSSI) |
| W3 | 0.2 | Signal quality (SNR) |
| W4 | 0.4 | Link reliability (ETX) |
| W5 | 1.0 | Gateway load penalty |

**Normalization:**
- RSSI: [-120, -30] dBm → [0, 1]
- SNR: [-20, 10] dB → [0, 1]
- Lower cost = better route

**Route Hysteresis:** New routes must be 15% better (`newCost < currentCost * 0.85`) to replace existing routes, preventing route flapping.

#### ETXTracker - Link Quality Estimation

Zero-overhead Expected Transmission Count (ETX) via sequence-gap detection.

**Algorithm:**
1. Track sequence numbers per neighbor
2. Detect gaps (missing packets) between expected and received
3. Record failures for each missing packet
4. Calculate delivery ratio from sliding window (10 samples)
5. ETX = 1 / delivery_ratio
6. Apply EWMA smoothing (α=0.3)

**Known Limitation:** Currently, the sequence number passed to ETXTracker is the local node's transmit counter, not the sender's sequence. This means ETX values are not meaningful for actual loss detection. Fix: Use `PacketHeader::id` from received packets.

#### TrickleScheduler - Adaptive HELLO Timing (RFC 6206)

Reduces control overhead in stable networks by 30-40%.

**Algorithm:**
1. Start with I = I_min (60s)
2. Pick random transmit time t in [I/2, I]
3. Count consistent HELLOs received (c)
4. At time t: transmit only if c < k (suppression)
5. At interval end: double I (up to I_max=600s), reset c

**Parameters (config.h):**
- `TRICKLE_I_MIN`: 60,000ms (1 minute)
- `TRICKLE_I_MAX`: 600,000ms (10 minutes)
- `TRICKLE_K`: 1 (suppress if ANY neighbor sent)

**Known Limitation:** Trickle currently paces application broadcasts, NOT LoRaMesher's built-in HELLO packets (which run on a fixed 120s timer). The claimed overhead reduction only applies to xMESH's custom broadcasts.

#### GatewayBalancer - Load Distribution & Failure Detection

**Load Tracking:**
- Counts packets forwarded per minute
- Encodes load (0-254) for HELLO advertisement

**Neighbor Health:**
- Warning threshold: 180s (1 missed HELLO)
- Failure threshold: 360s (2 missed HELLOs)
- Failed neighbors rejected in cost callback

**Known Limitation:** Gateway load is not actually transmitted in HELLO packets. The current implementation uses local gateway load regardless of which gateway is being evaluated. Multi-gateway load balancing is not fully functional.

#### MobilityDetector - Adaptive Parameters

Detects node movement via SNR variance analysis.

**States:**
| State | SNR Variance | Trickle Params | Detection Threshold |
|-------|--------------|----------------|---------------------|
| STATIC | Low (<4 dB²) | I=60-600s | 360s |
| MOBILE | High (>8 dB²) | I=20-120s | 180s |
| EMERGENCY | Triggered | I=10-60s | 90s |

**Usage:** Call `mobilityDetector.feedSNR(addr, snr)` on each HELLO, then `mobilityDetector.tick()` periodically.

### 2. xmesh-security: Encryption Layer

#### SecurityManager - Unified Security Facade

**Security Levels:**
```cpp
enum class SecurityLevel : uint8_t {
    NONE = 0,      // Plaintext (default)
    AUTH_ONLY = 1, // Device whitelist only (NOT enforced in production)
    ENCRYPTED = 2, // AES-GCM encryption + auth
    FULL = 3       // Encryption + frame counter replay protection
};
```

**Secure Packet Format:**
```
+-------------------+---------------------------+
| SecurePacketHeader|        Encrypted Data     |
+-------------------+---------------------------+
| version (1)       | nonce (12 bytes)          |
| keyVersion (1)    | ciphertext (N bytes)      |
| frameCounter (4)  | tag (4 bytes)             |
+-------------------+---------------------------+
Total overhead: 6 + 16 = 22 bytes
```

#### PayloadCrypto - AES-256-GCM Encryption

**Implementation:**
- Algorithm: AES-256-GCM (via mbedtls)
- Key size: 256 bits
- Nonce: 12 bytes [frameCounter(4) | nodeRandom(8)]
- Tag: 4 bytes (truncated from 16)

**Security Notes:**
- 4-byte tag provides ~2^32 forgery resistance (casual tampering only)
- nodeRandom is per-boot, creating theoretical nonce collision risk
- Recommend: Use device-unique prefix instead of random

#### KeyManager - Key Derivation

**PBKDF2-HMAC-SHA256:**
- Iterations: 10,000
- Salt: Fixed 16-byte value
- Output: 256-bit key

**Storage:** ESP32 NVS (`xmesh_keys` namespace)

**Rotation:** Supports current + previous key for graceful rotation

#### FrameCounter - Replay Protection

**Per-Node Counters:**
- 32-bit outgoing counter (persisted to NVS)
- Per-peer incoming counter tracking
- 32-packet sliding window for out-of-order

**Known Limitation:** Per-peer counters are NOT persisted. After reboot, replay attacks are possible until legitimate traffic re-establishes state.

### 3. xmesh-hal: Hardware Abstraction

#### Sensors - PMS7003 & GPS

**PMS7003 Air Quality:**
- UART1 (RX=4, TX=5), 9600 baud
- Readings: PM1.0, PM2.5, PM10 (μg/m³)
- Power management via SET pin (GPIO 3)
- 30-second warmup required

**GPS Module:**
- UART2 (RX=6, TX=7), 9600 baud
- TinyGPSPlus NMEA parsing
- Outputs: lat/lon, altitude, satellite count

**Node Mode Detection:**
```cpp
enum class NodeMode : uint8_t {
    RELAY = 0,    // No sensors detected
    SENSOR = 1,   // PMS and/or GPS detected
    GATEWAY = 2   // Gateway role enabled
};
```

#### SensorPacket - Wire Format

23-byte packed structure for mesh transmission:
```cpp
struct __attribute__((packed)) SensorPacket {
    uint8_t  version;     // Format version (1)
    uint8_t  flags;       // Validity flags
    uint16_t pm1_0;       // PM1.0 μg/m³
    uint16_t pm2_5;       // PM2.5 μg/m³
    uint16_t pm10;        // PM10 μg/m³
    int32_t  latitude;    // lat × 10^7
    int32_t  longitude;   // lon × 10^7
    int16_t  altitude;    // meters
    uint8_t  satellites;  // GPS sat count
    uint32_t timestamp;   // uptime ms
};
```

**Flags:**
- `0x01`: PMS data valid
- `0x02`: GPS data valid
- `0x04`: GPS has fix

### 4. xmesh-ota: Over-the-Air Updates

#### OTAManager - Dual-Mode Updates

**ArduinoOTA (LAN):**
- Hostname: `xmesh-gateway`
- Port: default (3232)
- Uses ESP-IDF OTA partition scheme

**HTTP OTA:**
- Version check from URL
- Firmware download with progress
- Automatic rollback on boot failure (3 attempts)

**Safety:**
- Boot counter persisted in NVS
- App marked valid after mesh operational
- Rollback to previous partition on failure

## Data Flow

### Packet Transmission Flow

```
1. Application creates SensorPacket
2. sendSecurePacket() called
   ├── SecurityManager.securePayload() [if level >= ENCRYPTED]
   │   ├── Add SecurePacketHeader
   │   ├── PayloadCrypto.encrypt() - AES-GCM
   │   └── FrameCounter.getNextOutgoing()
   └── LoraMesher.sendPacket()
       ├── PacketService.createDataPacket()
       ├── RoutingTableService.getNextHop()
       └── Radio transmit via SX1262
```

### Packet Reception Flow

```
1. RadioLib receives LoRa packet
2. LoraMesher.processPackets()
   ├── Decode packet type
   ├── If HELLO: processRoute()
   │   ├── Update routing table
   │   ├── Call helloCallback() → Trickle/ETX/GatewayBalancer
   │   └── costCallback() for route comparison
   └── If DATA: forward or deliver
3. For local delivery:
   ├── Notify ReceiveAppData_TaskHandle
   └── processReceivedPackets() task
       ├── SecurityManager.verifyAndDecrypt() [if level >= ENCRYPTED]
       ├── Parse SensorPacket
       └── If gateway: MQTT publish
```

### Routing Decision Flow

```
1. HELLO packet arrives with routing info
2. RoutingTableService.processRoute()
3. For each advertised node:
   ├── Check if route exists
   ├── If costCallback set:
   │   ├── Calculate newCost = costCalculationCallback(metric, via, addr)
   │   ├── Calculate currentCost for existing route
   │   └── Update if newCost < currentCost * 0.85
   └── Else: use hop count comparison
```

## FreeRTOS Task Structure

| Task | Stack | Priority | Purpose |
|------|-------|----------|---------|
| Hello_Task | 4096 | 1 | Send HELLO packets (120s) |
| ReceivePacket_Task | 4096 | 2 | Handle radio interrupts |
| ReceiveData_Task | 4096 | 2 | Process received packets |
| SendData_Task | 4096 | 2 | Transmit queued packets |
| ReceiveAppData_Task | 4096 | 2 | Application packet handler |
| QueueManager_Task | 4096 | 1 | Timeout management |
| RoutingTableMgr_Task | 4096 | 1 | Route expiration |

## Configuration Reference (config.h)

### Trickle Parameters
| Parameter | Default | Description |
|-----------|---------|-------------|
| TRICKLE_I_MIN | 60,000 ms | Minimum HELLO interval |
| TRICKLE_I_MAX | 600,000 ms | Maximum HELLO interval |
| TRICKLE_K | 1 | Redundancy constant |
| TRICKLE_ENABLED | true | Enable adaptive scheduling |

### Cost Router Weights
| Parameter | Default | Description |
|-----------|---------|-------------|
| W1_HOP_COUNT | 1.0 | Hop count weight |
| W2_RSSI | 0.3 | RSSI weight |
| W3_SNR | 0.2 | SNR weight |
| W4_ETX | 0.4 | ETX weight |
| W5_GATEWAY_BIAS | 1.0 | Gateway load weight |

### Hardware Pins (Heltec V3)
| Pin | GPIO | Function |
|-----|------|----------|
| LoRa CS | 8 | SPI chip select |
| LoRa RST | 12 | Radio reset |
| LoRa IRQ | 14 | DIO1 interrupt |
| LoRa BUSY | 13 | SX1262 busy |
| OLED SDA | 17 | I2C data |
| OLED SCL | 18 | I2C clock |
| OLED RST | 21 | Display reset |
| Vext | 36 | OLED power |
| PMS RX | 4 | Air quality sensor |
| PMS TX | 5 | Air quality sensor |
| GPS RX | 6 | GPS module |
| GPS TX | 7 | GPS module |

## Testing

### Unit Tests (Unity Framework)

Location: `firmware/production/test/test_native/`

| Test Suite | Coverage |
|------------|----------|
| test_cost_router | Normalization, cost formula |
| test_etx_tracker | Sequence gaps, EWMA |
| test_trickle_scheduler | Suppression, interval doubling |
| test_gateway_balancer | Load encoding, failure detection |
| test_mobility_detector | State transitions |
| test_security_manager | Encrypt/decrypt, replay |
| test_key_manager | PBKDF2, key rotation |
| test_payload_crypto | AES-GCM |
| test_sensors | Detection, power management |
| test_ota_manager | Version check, rollback |

**Run tests:**
```bash
cd firmware/production
pio test -e native
```

## Known Limitations

### Routing
1. **ETX Not Functional:** Sequence numbers not properly wired from packet headers
2. **Gateway Load Not Propagated:** HELLO packets don't carry load field
3. **Trickle Scope:** Only paces application broadcasts, not LoRaMesher HELLOs
4. **New Neighbor Detection:** Race condition causes inconsistent reset

### Security
1. **4-byte GCM Tag:** Weak against active attackers (use for tampering detection only)
2. **Replay After Reboot:** Per-peer counters not persisted
3. **Shared Network Key:** One compromised node compromises all
4. **AUTH_ONLY Not Enforced:** Production main.cpp only gates on ENCRYPTED+

### General
1. **Single Radio Frequency:** No channel hopping
2. **No Mesh Routing Visualization:** Status via serial only
3. **Limited Gateway Discovery:** Relies on HELLO broadcast range

## Build Commands

```bash
# Build production firmware
cd firmware/production
pio run

# Flash via USB
pio run -t upload

# Flash via OTA
pio run -e ota -t upload --upload-port 192.168.1.x

# Run unit tests
pio test -e native

# Start MQTT broker for gateway testing
cd mqtt-setup
docker compose up -d
```

## See Also

- [FORK_MODIFICATION.md](FORK_MODIFICATION.md) - Details on LoRaMesher changes
- [AGENTS.md](AGENTS.md) - AI agent knowledge base
- [LoRaMesher Documentation](https://github.com/LoRaMesher/LoRaMesher)
