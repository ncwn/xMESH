# Firmware Development Guide

## Project Structure

```
firmware/
├── 1_flooding/                 # Protocol 1: Pure flooding baseline
│   ├── platformio.ini          # PlatformIO configuration
│   └── src/
│       ├── main.cpp           # Main application
│       ├── config.h           # Configuration parameters
│       ├── display.cpp/h      # OLED display management
│       └── flooding.cpp/h     # Flooding protocol implementation
│
├── 2_hopcount/                 # Protocol 2: LoRaMesher hop-count routing
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── config.h
│       ├── display.cpp/h
│       └── mesh_handler.cpp/h # LoRaMesher integration
│
├── 3_gateway_routing/          # Protocol 3: Cost-aware gateway routing
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── config.h
│       ├── display.cpp/h
│       ├── cost_routing.cpp/h # Cost-based routing logic
│       ├── trickle.cpp/h      # Trickle timer implementation
│       └── metrics.cpp/h      # Link quality metrics (RSSI, SNR, ETX)
│
└── common/                     # Shared utilities
    ├── heltec_v3_pins.h       # Pin definitions for Heltec V3
    ├── display_utils.cpp/h    # Common display functions
    ├── logging.cpp/h          # Serial logging with timestamps
    └── duty_cycle.cpp/h       # Duty cycle tracking
```

## Development Workflow

### 1. Environment Setup

```bash
# Clone the repository
git clone https://github.com/ncwn/xMESH.git
cd xMESH

# Install PlatformIO
pip install platformio

# Install VS Code PlatformIO extension (recommended)
code --install-extension platformio.platformio-ide
```

### 2. Firmware Selection

Each protocol has its own firmware directory:

```bash
# Navigate to desired protocol
cd firmware/1_flooding    # For flooding protocol
cd firmware/2_hopcount    # For hop-count routing
cd firmware/3_gateway_routing  # For cost-aware routing
```

### 3. Configuration

Edit `src/config.h` for each firmware:

```cpp
// config.h - Example configuration

#ifndef CONFIG_H
#define CONFIG_H

// Node Configuration
#define NODE_ID 1                    // Unique node identifier (1-5)
#define NODE_ROLE ROLE_SENSOR        // ROLE_SENSOR, ROLE_RELAY, ROLE_GATEWAY
#define NODE_ADDRESS 0x0001          // Unique 16-bit address

// LoRa Configuration (AS923 Thailand)
#define LORA_FREQUENCY 923.2         // MHz
#define LORA_BANDWIDTH 125.0         // kHz
#define LORA_SPREADING_FACTOR 7     // SF7-SF12
#define LORA_CODING_RATE 5           // 5-8 (4/5 to 4/8)
#define LORA_SYNC_WORD 0x12          // Network identifier
#define LORA_TX_POWER 10             // dBm (indoor validation default; drop to 6-8 dBm for hallway multi-hop)
#define LORA_PREAMBLE_LENGTH 8       // symbols

// Protocol Configuration
#define PACKET_INTERVAL_MS 60000     // Send interval (60 seconds)
#define HELLO_INTERVAL_MS 120000     // Routing update interval
#define ROUTE_TIMEOUT_MS 600000      // Route expiration (10 minutes)
#define MAX_RETRIES 3                // Retransmission attempts
#define ACK_TIMEOUT_MS 2000          // ACK wait time

// Display Configuration
#define DISPLAY_UPDATE_MS 1000       // Display refresh rate
#define DISPLAY_TIMEOUT_MS 30000     // Display sleep timeout

// Logging Configuration
#define SERIAL_BAUD 115200
#define LOG_LEVEL LOG_DEBUG          // LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG
#define CSV_OUTPUT true              // Enable CSV format for data collection

// Duty Cycle Enforcement
#define DUTY_CYCLE_LIMIT_MS 36000    // 1% of 3600000ms (1 hour)
#define DUTY_CYCLE_WARNING_MS 30000  // Warn at 83% usage

#endif // CONFIG_H
```

### 4. Building Firmware

#### Using PlatformIO CLI

```bash
# Build firmware
pio run

# Clean build
pio run --target clean

# Build with specific environment
pio run -e heltec_wifi_lora_32_V3

# Build all protocols (from root directory)
for dir in firmware/*/; do
    cd "$dir"
    pio run
    cd ../..
done
```

#### Using VS Code
1. Open firmware directory in VS Code
2. PlatformIO extension auto-detects `platformio.ini`
3. Click "Build" button in status bar
4. Or use Command Palette: `PlatformIO: Build`

### 5. Flashing Firmware

#### Single Board

```bash
# Upload to board
pio run --target upload

# Specify port explicitly
pio run --target upload --upload-port /dev/ttyUSB0

# Upload and monitor
pio run --target upload && pio device monitor
```

#### Multiple Boards (Batch Flashing)

Create `flash_nodes.sh`:

```bash
#!/bin/bash
# flash_nodes.sh - Flash multiple nodes with different IDs

FIRMWARE_DIR="firmware/3_gateway_routing"
PORTS=("/dev/ttyUSB0" "/dev/ttyUSB1" "/dev/ttyUSB2")
NODE_IDS=(1 2 3)

for i in ${!PORTS[@]}; do
    echo "Flashing Node ${NODE_IDS[$i]} on ${PORTS[$i]}"

    # Build with node-specific defines
    cd $FIRMWARE_DIR
    pio run --target upload \
        --upload-port ${PORTS[$i]} \
        -D NODE_ID=${NODE_IDS[$i]}

    echo "Node ${NODE_IDS[$i]} flashed successfully"
done
```

### 6. Serial Monitoring

#### Basic Monitoring

```bash
# Monitor single board
pio device monitor --baud 115200

# Monitor with filters
pio device monitor --baud 115200 --filter colorize --filter time

# Monitor specific port
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

#### Multi-Node Monitoring

Use `screen` or `tmux` for multiple terminals:

```bash
# Terminal 1
screen /dev/ttyUSB0 115200

# Terminal 2
screen /dev/ttyUSB1 115200

# Terminal 3
screen /dev/ttyUSB2 115200
```

Or use Python script for consolidated logging:

```python
# monitor_all.py
import serial
import threading
from datetime import datetime

def monitor_port(port, node_id):
    ser = serial.Serial(port, 115200, timeout=1)
    while True:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8').strip()
            timestamp = datetime.now().isoformat()
            print(f"[{timestamp}] Node{node_id}: {line}")

# Start monitoring threads
ports = [
    ("/dev/ttyUSB0", 1),
    ("/dev/ttyUSB1", 2),
    ("/dev/ttyUSB2", 3),
]

threads = []
for port, node_id in ports:
    t = threading.Thread(target=monitor_port, args=(port, node_id))
    t.daemon = True
    t.start()
    threads.append(t)

# Keep main thread alive
for t in threads:
    t.join()
```

## Protocol Implementation Details

### Protocol 1: Flooding

```cpp
// flooding.cpp - Simplified flooding implementation

void handlePacket(Packet* packet) {
    // Check duplicate cache
    if (isDuplicate(packet->src, packet->seq)) {
        return;  // Already processed
    }

    // Add to cache
    addToCache(packet->src, packet->seq);

    // Process packet if we're destination
    if (packet->dest == NODE_ADDRESS || packet->dest == BROADCAST_ADDR) {
        processPacket(packet);
    }

    // Rebroadcast if not gateway
    if (NODE_ROLE != ROLE_GATEWAY) {
        packet->ttl--;
        if (packet->ttl > 0) {
            transmitPacket(packet);
        }
    }
}
```

### Protocol 2: Hop-Count Routing

**Implementation**: Uses LoRaMesher library for distance-vector routing

**Key Behavior (Verified in Hardware Testing):**
- HELLO packets every 120 seconds build routing tables
- All nodes participate in routing (not role-restricted)
- Forwarding decision: `if (packet.via == myAddress)` then forward, else drop
- Sensors CAN forward if they are on the routing path
- Gateway never forwards (always final destination)
- Shortest path automatically selected

**Hardware Validation Results:**
- ✅ 100% PDR achieved
- ✅ Route convergence <30 seconds
- ✅ Direct path optimization working
- ✅ Unicast forwarding verified

```cpp
// mesh_handler.cpp - LoRaMesher integration

#include <LoraMesher.h>

LoraMesher& mesh = LoraMesher::getInstance();

void setupMesh() {
    // Configure LoRaMesher
    LoraMesher::LoraMesherConfig config;
    config.loraCs = LORA_CS_PIN;
    config.loraRst = LORA_RST_PIN;
    config.loraIrq = LORA_DIO1_PIN;
    config.loraIo1 = LORA_DIO1_PIN;
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    config.freq = LORA_FREQUENCY;
    config.bw = LORA_BANDWIDTH;
    config.sf = LORA_SPREADING_FACTOR;
    config.power = LORA_TX_POWER;

    // Initialize mesh
    mesh.begin(config);

    // Set role (gateway vs default)
    if (NODE_ROLE == XMESH_ROLE_GATEWAY) {
        mesh.addGatewayRole();  // Advertise as gateway
    }

    // Add received callback (ALL nodes receive)
    mesh.onReceive([](DataPacket* packet) {
        processReceivedPacket(packet);
    });
}
```

### Protocol 3: Cost-Aware Gateway Routing

```cpp
// cost_routing.cpp - Cost calculation and routing

struct LinkMetrics {
    float rssi;
    float snr;
    float etx;
    uint8_t hopCount;
};

float calculateCost(LinkMetrics& metrics, bool isGateway) {
    // Normalize RSSI (-120 to -30 dBm range)
    float rssiNorm = (metrics.rssi + 120) / 90.0;
    rssiNorm = constrain(rssiNorm, 0.0, 1.0);

    // Normalize SNR (-20 to +10 dB range)
    float snrNorm = (metrics.snr + 20) / 30.0;
    snrNorm = constrain(snrNorm, 0.0, 1.0);

    // Calculate weighted cost
    float cost = 0;
    cost += 1.0 * metrics.hopCount;          // W1: hop count
    cost += 0.3 * (1.0 - rssiNorm);         // W2: RSSI penalty
    cost += 0.2 * (1.0 - snrNorm);          // W3: SNR penalty
    cost += 0.4 * metrics.etx;              // W4: ETX penalty

    // Gateway bias
    if (isGateway) {
        cost -= 1.0;  // W5: prefer gateway paths
    }

    return cost;
}

RouteNode* selectBestRoute(uint16_t destination) {
    RouteNode* bestRoute = nullptr;
    float minCost = INFINITY;

    for (auto& route : routingTable) {
        if (route.destination == destination) {
            LinkMetrics metrics = getMetricsForNode(route.nextHop);
            float cost = calculateCost(metrics, route.role == ROLE_GATEWAY);

            // Apply hysteresis (15% threshold)
            if (cost < minCost * 0.85) {
                minCost = cost;
                bestRoute = &route;
            }
        }
    }

    return bestRoute;
}
```

## Display Integration

### OLED Display Layout

```
┌──────────────────────┐
│ Node 1 - SENSOR      │  Line 1: ID & Role
│ RSSI:-65 SNR:8.5     │  Line 2: Link metrics
│ Pkts: TX:42 RX:156   │  Line 3: Packet counters
│ Routes: 3 Cost:2.4   │  Line 4: Routing info
│ Duty: 0.4% [====  ]  │  Line 5: Duty cycle
│ Uptime: 01:23:45     │  Line 6: Uptime
│ GW:0x04 Via:0x02     │  Line 7: Gateway & next hop
│ Status: OK           │  Line 8: Status message
└──────────────────────┘
```

### Display Implementation

```cpp
// display.cpp - OLED display management

#include <Adafruit_SSD1306.h>

class DisplayManager {
private:
    Adafruit_SSD1306 display;
    unsigned long lastUpdate;

public:
    void init() {
        Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
    }

    void update(NodeStatus& status) {
        if (millis() - lastUpdate < DISPLAY_UPDATE_MS) {
            return;
        }

        display.clearDisplay();
        display.setCursor(0, 0);

        // Line 1: Node info
        display.printf("Node %d - %s\n", NODE_ID, roleToString(NODE_ROLE));

        // Line 2: Link metrics
        display.printf("RSSI:%.0f SNR:%.1f\n", status.rssi, status.snr);

        // Line 3: Packet stats
        display.printf("Pkts: TX:%lu RX:%lu\n", status.txCount, status.rxCount);

        // Line 4: Routing
        display.printf("Routes:%d Cost:%.1f\n", status.routeCount, status.cost);

        // Line 5: Duty cycle bar
        drawDutyCycleBar(status.dutyCycle);

        // Line 6: Uptime
        displayUptime(status.uptimeMs);

        // Line 7: Gateway info
        display.printf("GW:0x%02X Via:0x%02X\n", status.gateway, status.nextHop);

        // Line 8: Status
        display.printf("Status: %s", status.message);

        display.display();
        lastUpdate = millis();
    }

    void drawDutyCycleBar(float percentage) {
        display.printf("Duty: %.1f%% ", percentage);

        // Draw progress bar
        int barWidth = 40;
        int filled = (percentage / 100.0) * barWidth;

        display.print("[");
        for (int i = 0; i < barWidth; i++) {
            display.print(i < filled ? "=" : " ");
        }
        display.println("]");
    }

    void displayUptime(unsigned long ms) {
        unsigned long seconds = ms / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;

        display.printf("Uptime: %02lu:%02lu:%02lu\n",
            hours, minutes % 60, seconds % 60);
    }
};
```

## Logging Format

### CSV Output for Data Collection

```cpp
// logging.cpp - Structured logging

void logPacketEvent(PacketEvent& event) {
    if (!CSV_OUTPUT) {
        Serial.printf("[%s] %s\n", getTimestamp(), event.toString());
        return;
    }

    // CSV format for data analysis
    Serial.printf("%lu,%d,%s,%d,%d,%.1f,%.1f,%.2f,%d,%d\n",
        millis(),                    // timestamp
        NODE_ID,                     // node_id
        eventTypeToString(event.type), // event_type
        event.src,                   // src_address
        event.dest,                  // dest_address
        event.rssi,                  // rssi
        event.snr,                   // snr
        event.etx,                   // etx
        event.hopCount,              // hop_count
        event.packetSize             // packet_size
    );
}

// CSV Header (printed on startup)
void printCSVHeader() {
    Serial.println("timestamp,node_id,event_type,src,dest,rssi,snr,etx,hop_count,packet_size");
}
```

## Testing Procedures

### Unit Testing

Each firmware component should be tested individually:

```bash
# Run unit tests
cd firmware/3_gateway_routing
pio test

# Test specific component
pio test -f test_trickle
pio test -f test_metrics
pio test -f test_cost_calculation
```

### Integration Testing

Test complete protocol operation:

1. **Setup**: Flash 3 nodes with different IDs
2. **Configuration**: Set Node 3 as gateway
3. **Test**: Send packets from Node 1 to gateway
4. **Verify**: Check routing paths and metrics

### Hardware Validation Gates

Before documenting results, verify:

- [ ] All nodes communicate successfully
- [ ] OLED displays show correct information
- [ ] Serial logging outputs CSV format
- [ ] Duty cycle stays under 1%
- [ ] Routes converge within 5 HELLO intervals
- [ ] Cost metrics reflect link quality
- [ ] Gateway preference working
- [ ] Packet delivery rate > 95%

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   ```
   Solution: Check library dependencies in platformio.ini
   Verify RadioLib version >= 7.1.0
   ```

2. **Upload Failures**
   ```
   Solution: Manual flash mode (hold PRG + press RST)
   Check USB cable and drivers
   ```

3. **No Communication**
   ```
   Solution: Verify antenna connected
   Check frequency settings (923.2 MHz)
   Confirm sync word matches
   ```

4. **Display Not Working**
   ```
   Solution: Check I2C address (0x3C or 0x3D)
   Verify Wire.begin() with correct pins
   ```

5. **High Packet Loss**
   ```
   Solution: Reduce TX power if too close
   Check duty cycle not exceeded
   Verify spreading factor settings
   ```

## Performance Optimization

### Memory Management

```cpp
// Use PROGMEM for constants
const char STATUS_OK[] PROGMEM = "OK";

// Preallocate buffers
uint8_t packetBuffer[255];

// Use stack instead of heap where possible
void processPacket() {
    PacketHeader header;  // Stack allocation
    // vs
    PacketHeader* header = new PacketHeader();  // Avoid
}
```

### Power Optimization

```cpp
// Deep sleep between transmissions
void enterDeepSleep(uint32_t seconds) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
}

// Reduce CPU frequency when idle
setCpuFrequencyMhz(80);  // From 240 MHz
```

### Radio Optimization

```cpp
// Adaptive data rate
void adjustDataRate(float packetLoss) {
    if (packetLoss > 0.1) {
        // Increase spreading factor for reliability
        radio.setSpreadingFactor(8);
    } else if (packetLoss < 0.02) {
        // Decrease for speed
        radio.setSpreadingFactor(7);
    }
}
```

## Version Management

### Firmware Versioning

```cpp
// version.h
#define FIRMWARE_VERSION "1.2.3"
#define PROTOCOL_VERSION 3
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Report version on startup
Serial.printf("Firmware v%s Protocol %d Built %s %s\n",
    FIRMWARE_VERSION, PROTOCOL_VERSION, BUILD_DATE, BUILD_TIME);
```

### Git Workflow

```bash
# Create feature branch
git checkout -b feature/implement-etx-tracking

# Commit changes
git add .
git commit -m "feat: Add ETX tracking with sequence-aware logic"

# Tag releases
git tag -a v1.0.0 -m "Protocol 3 initial implementation"
git push origin --tags
```

## Documentation Requirements

After successful hardware testing:

1. Update CHANGELOG.md with test results
2. Document performance metrics in CSV format
3. Create test report with statistical analysis
4. Update README with latest findings
5. Archive raw data in experiments/results/

Remember: **Never update documentation before hardware validation!**
