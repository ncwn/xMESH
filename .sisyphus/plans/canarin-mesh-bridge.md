# Canarin LoRa Mesh Bridge

## TL;DR

> **Quick Summary**: Extend Canarin environmental sensor network into remote/no-cellular regions by adding Heltec ESP32-S3 nodes as LoRa mesh relays. Phase 1 uses AT command emulation (zero Canarin changes). Phase 2 adds gateway MQTT publishing. Phase 3 optionally adds optimized binary protocol.
> 
> **Deliverables**:
> - `lib/xmesh-canarin-bridge/` - AT command emulator library
> - Bridge mode firmware configuration
> - Gateway MQTT publisher with can5_lmsg decoder
> - TTN MQTT bridge configuration (optional)
> - Integration tests and documentation
> 
> **Estimated Effort**: Large (3-4 weeks)
> **Parallel Execution**: YES - 3 waves
> **Critical Path**: Task 1 → Task 3 → Task 5 → Task 8 → Task 10

---

## Context

### Original Request
User wants to add LoRa Mesh capability to extend Canarin air quality sensor network into remote forest/mountain areas without cellular coverage. Heltec WiFi LoRa 32 V3 modules will act as mesh relays, receiving sensor data from Canarin boards via UART and forwarding through LoRaMesher network to a gateway that publishes to MQTT/TTN.

### Interview Summary
**Key Discussions**:
- Canarin uses PMS7003, GPS, BME280, gas sensors on ESP32/ESP32-S3
- Currently uses either 4G (SIM7600) or LoRaWAN (LoRa-E5) via NETPORT_1 @ 9600 baud
- Heltec replaces LoRa-E5 module, connects via same UART
- Bridge nodes are relay-only (no sensors initially)
- TTN integration needed for existing backend compatibility

**Research Findings**:
- LEGO uses `can5_lmsg` binary format for LoRa payloads
- AT commands documented in `can5_netif_lwan.c`: VER, JOIN, NJS, SEND, CTX, DEUI, etc.
- **GPIO 32/35 don't exist on ESP32-S3** - need different pins
- Heltec V3 has GPIO 1-48 available (ESP32-S3)

### Metis Review
**Identified Gaps** (addressed):
- GPIO pin mismatch: Use GPIO 19/20 or other available pins
- AT command completeness: Implement subset used by Canarin (VER, JOIN, NJS, SEND, CTX)
- UART availability: Bridge mode reuses UART1 (no PMS7003 in relay-only mode)
- Mesh latency: Fake immediate ACK, async mesh delivery
- can5_lmsg format: Fully documented in codebase

---

## Work Objectives

### Core Objective
Create a LoRa mesh bridge system that extends Canarin sensor network range using Heltec ESP32-S3 nodes as mesh relays, with gateway forwarding to MQTT/TTN.

### Concrete Deliverables
- `lib/xmesh-canarin-bridge/` library with AT command emulator
- `CanarinMessage` struct for can5_lmsg format parsing
- Bridge mode compile flag in platformio.ini
- Gateway MQTT publisher module
- TTN MQTT bridge documentation/configuration
- Unit tests for AT parser and message decoder
- Integration test with 2+ nodes

### Definition of Done
- [ ] Bridge node receives AT+SEND from Canarin, forwards to mesh
- [ ] Gateway receives mesh packet, publishes to MQTT as JSON
- [ ] All unit tests pass: `pio test -e native`
- [ ] Integration test with real hardware succeeds
- [ ] No Canarin firmware modifications required (Phase 1)

### Must Have
- AT+VER detection response
- AT+JOIN emulation (fake success)
- AT+NJS status (always joined)
- AT+SEND payload extraction and mesh forwarding
- MQTT JSON output at gateway
- Configurable UART pins

### Must NOT Have (Guardrails)
- DO NOT modify Canarin firmware (Phase 1)
- DO NOT implement full LoRaWAN stack
- DO NOT add sensors to bridge nodes (Phase 1)
- DO NOT implement bidirectional TTN downlink (defer to Phase 3)
- DO NOT add web configuration UI
- DO NOT buffer more than 10 messages (minimal RAM usage)
- DO NOT implement all AT commands - only those Canarin uses

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: YES (Unity framework in `firmware/production/test/`)
- **User wants tests**: YES (TDD for AT parser, integration tests)
- **Framework**: Unity (native tests via PlatformIO)

### TDD Structure
Each core module follows RED-GREEN-REFACTOR:
1. Write failing test first
2. Implement minimum code to pass
3. Refactor while keeping green

### Test Files
- `firmware/production/test/test_native/test_at_parser/test_at_parser.cpp` - AT command parsing
- `firmware/production/test/test_native/test_canarin_message/test_canarin_message.cpp` - can5_lmsg decoding
- `firmware/production/test/test_native/test_uart_bridge/test_uart_bridge.cpp` - UART state machine

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately):
├── Task 1: AT Command Parser module
├── Task 2: CanarinMessage struct and decoder
└── Task 4: UART Bridge state machine

Wave 2 (After Wave 1):
├── Task 3: Mesh integration (depends: 1, 2)
├── Task 5: Bridge mode config (depends: 4)
└── Task 6: Gateway receiver module

Wave 3 (After Wave 2):
├── Task 7: MQTT publisher (depends: 6)
├── Task 8: Integration test (depends: 3, 5)
└── Task 9: TTN bridge documentation

Final:
└── Task 10: Phase 2 - New binary protocol (optional, after all above)
```

### Dependency Matrix

| Task | Depends On | Blocks | Can Parallelize With |
|------|------------|--------|---------------------|
| 1 | None | 3 | 2, 4 |
| 2 | None | 3, 7 | 1, 4 |
| 3 | 1, 2 | 8 | 5, 6 |
| 4 | None | 5 | 1, 2 |
| 5 | 4 | 8 | 3, 6 |
| 6 | None | 7 | 3, 5 |
| 7 | 2, 6 | 9 | 8 |
| 8 | 3, 5 | 10 | 7 |
| 9 | 7 | None | 8 |
| 10 | 8 | None | None (Phase 2) |

---

## TODOs

### Phase 1: AT Emulation (Zero Canarin Changes)

- [ ] 1. Create AT Command Parser Module

  **What to do**:
  - Create `lib/xmesh-canarin-bridge/src/ATCommandParser.h/cpp`
  - Implement state machine to parse AT commands from UART stream
  - Handle commands: AT+VER, AT+JOIN, AT+NJS, AT+SEND, AT+CTX, AT+DEUI
  - Generate appropriate responses (OK, +EVT:JOINED, etc.)
  - Write unit tests covering all commands

  **Must NOT do**:
  - Implement full LoRaWAN AT command set
  - Connect to real LoRaWAN network

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Parser with state machine, requires careful design
  - **Skills**: [`git-master`]
    - `git-master`: Atomic commits per AT command implemented

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2, 4)
  - **Blocks**: Task 3
  - **Blocked By**: None

  **References**:
  - `.canarinv5-LEGO/components/can5_net/can5_netif/can5_netif_lwan/can5_netif_lwan.c:176-226` - AT command definitions
  - `.canarinv5-LEGO/components/can5_net/can5_netif/can5_netif_lwan/can5_netif_lwan.c:77-89` - Response codes
  - `.canarinv5-LEGO/components/can5_net/can5_netif/can5_netif_lwan/can5_netif_lwan.c:109-116` - Event strings
  - `lib/xmesh-security/src/SecurityManager.cpp` - Singleton pattern to follow
  - `firmware/production/test/test_native/test_cost_router/test_cost_router.cpp` - Test file pattern

  **Acceptance Criteria**:
  - [ ] Test file created: `firmware/production/test/test_native/test_at_parser/test_at_parser.cpp`
  - [ ] Test covers: AT+VER returns version string containing "xMESH"
  - [ ] Test covers: AT+JOIN=1 triggers +EVT:JOINED response
  - [ ] Test covers: AT+SEND=21:0:ABCD extracts port=21, confirmed=0, hex="ABCD"
  - [ ] Test covers: Malformed command returns +ERR
  - [ ] `pio test -e native -f test_at_parser` -> PASS (run from firmware/production/)

  **Commit**: YES
  - Message: `feat(canarin-bridge): add AT command parser with state machine`
  - Files: `lib/xmesh-canarin-bridge/src/ATCommandParser.*`, `firmware/production/test/test_native/test_at_parser/test_at_parser.cpp`
  - Pre-commit: `pio test -e native -f test_at_parser`

---

- [ ] 2. Create CanarinMessage Struct and Decoder

  **What to do**:
  - Create `lib/xmesh-canarin-bridge/src/CanarinMessage.h/cpp`
  - Define `CanarinMessage` struct matching `can5_lmsg` wire format
  - Implement `decode(const uint8_t* hex, size_t len)` to parse hex payload
  - Implement `SensorReading` struct for individual sensor values
  - Write unit tests with known hex payloads

  **Must NOT do**:
  - Use xMESH SensorPacket format (different structure)
  - Implement encoding (only decode needed for bridge)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Binary protocol parsing, bit manipulation
  - **Skills**: [`git-master`]
    - `git-master`: Atomic commits

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 4)
  - **Blocks**: Tasks 3, 7
  - **Blocked By**: None

  **References**:
  - `.canarinv5-LEGO/components/can5_net/can5_protocols/can5_lorarelay/can5_loramsg.h:47-77` - Packet structures
  - `.canarinv5-LEGO/components/can5_net/can5_protocols/can5_lorarelay/can5_loramsg.h:12-43` - Sensor type enum
  - `.canarinv5-LEGO/components/can5_net/can5_protocols/can5_lorarelay/can5_codec_lwan.c:37-75` - Sensor type mapping
  - `lib/xmesh-hal/include/xmesh/hal/SensorPacket.h` - Similar struct pattern (but different format)

  **Acceptance Criteria**:
  - [ ] Test file created: `firmware/production/test/test_native/test_canarin_message/test_canarin_message.cpp`
  - [ ] Test covers: Single packet header parsing (is_single=1, timestamp extraction)
  - [ ] Test covers: Multi packet header parsing (is_single=0, n_data extraction)
  - [ ] Test covers: Sensor data extraction (type, value, sign)
  - [ ] Test covers: Known hex payload from LEGO produces expected values
  - [ ] `pio test -e native -f test_canarin_message` -> PASS (run from firmware/production/)

  **Commit**: YES
  - Message: `feat(canarin-bridge): add CanarinMessage decoder for can5_lmsg format`
  - Files: `lib/xmesh-canarin-bridge/src/CanarinMessage.*`, `firmware/production/test/test_native/test_canarin_message/test_canarin_message.cpp`
  - Pre-commit: `pio test -e native -f test_canarin_message`

---

- [ ] 3. Integrate AT Parser with LoRaMesher

  **What to do**:
  - Create `lib/xmesh-canarin-bridge/src/CanarinBridge.h/cpp`
  - Connect ATCommandParser to CanarinMessage decoder
  - On AT+SEND: decode hex, create mesh packet, call LoraMesher::sendPacket()
  - Queue mechanism for async mesh TX (don't block UART)
  - Respond with OK immediately, mesh delivery is fire-and-forget

  **Must NOT do**:
  - Wait for mesh ACK before responding
  - Implement mesh-level retries in bridge

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Integration of multiple components
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 8
  - **Blocked By**: Tasks 1, 2

  **References**:
  - `src/LoraMesher.h:253` - `sendPacket(dst, payload, size)` public API
  - `firmware/production/src/main.cpp:89-130` - sendSecurePacket pattern
  - `lib/xmesh-canarin-bridge/src/ATCommandParser.h` - Input (from Task 1)
  - `lib/xmesh-canarin-bridge/src/CanarinMessage.h` - Decoder (from Task 2)

  **Acceptance Criteria**:
  - [ ] `CanarinBridge::begin(HardwareSerial&)` initializes UART and parser
  - [ ] `CanarinBridge::loop()` processes incoming AT commands
  - [ ] AT+SEND triggers `LoraMesher::sendPacket()` call
  - [ ] Response timing < 100ms (before mesh TX completes)
  - [ ] Manual test: Serial inject AT+SEND, observe mesh TX on second node

  **Commit**: YES
  - Message: `feat(canarin-bridge): integrate AT parser with LoRaMesher mesh TX`
  - Files: `lib/xmesh-canarin-bridge/src/CanarinBridge.*`
  - Pre-commit: `pio test -e native`

---

- [ ] 4. UART Bridge State Machine

  **What to do**:
  - Create `lib/xmesh-canarin-bridge/src/UARTHandler.h/cpp`
  - Manage UART initialization with configurable pins
  - Handle receive buffer with line detection (\r\n terminator)
  - Implement TX queue for responses
  - Add timeout handling for incomplete commands

  **Must NOT do**:
  - Use fixed GPIO pins (must be configurable)
  - Block on UART operations

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard UART handling pattern
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2)
  - **Blocks**: Task 5
  - **Blocked By**: None

  **References**:
  - `firmware/production/src/main.cpp:48-49` - HardwareSerial pattern
  - `.canarinv5-LEGO/components/can5_hal/src/can5_hal.c:113-120` - UART config (9600 baud)
  - Arduino HardwareSerial documentation

  **Acceptance Criteria**:
  - [ ] `UARTHandler::begin(rxPin, txPin, baud)` configures UART
  - [ ] `UARTHandler::available()` returns pending bytes
  - [ ] `UARTHandler::readLine()` returns complete line or nullptr
  - [ ] `UARTHandler::write(const char*)` sends response
  - [ ] Works with any GPIO pair (not hardcoded)

  **Commit**: YES
  - Message: `feat(canarin-bridge): add configurable UART handler`
  - Files: `lib/xmesh-canarin-bridge/src/UARTHandler.*`
  - Pre-commit: `pio build`

---

- [ ] 5. Bridge Mode Configuration

  **What to do**:
  - Add `XMESH_BRIDGE_MODE` build flag to platformio.ini
  - Create [env:bridge] configuration that disables sensors
  - Add NVS config for UART pins: `bridge_rx_pin`, `bridge_tx_pin`
  - Update main.cpp to initialize CanarinBridge in bridge mode
  - Disable PMS7003/GPS when in bridge mode

  **Must NOT do**:
  - Require Canarin firmware changes
  - Break existing sensor node functionality

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Configuration changes only
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 3, 6)
  - **Blocks**: Task 8
  - **Blocked By**: Task 4

  **References**:
  - `firmware/production/platformio.ini` - Build configuration
  - `firmware/production/include/config.h` - Configuration pattern
  - `firmware/production/src/main.cpp:344-402` - NVS config pattern

  **Acceptance Criteria**:
  - [ ] `pio run -e bridge` builds without errors
  - [ ] Bridge mode skips sensor initialization
  - [ ] NVS stores `bridge_rx_pin`, `bridge_tx_pin`
  - [ ] Serial monitor shows "Bridge mode enabled" on boot

  **Commit**: YES
  - Message: `feat(config): add bridge mode build configuration`
  - Files: `platformio.ini`, `config.h`, `main.cpp`
  - Pre-commit: `pio run -e bridge && pio run`

---

- [ ] 6. Gateway Receiver Module

  **What to do**:
  - Create `lib/xmesh-canarin-bridge/src/GatewayReceiver.h/cpp`
  - Register task handle with `LoraMesher::setReceiveAppDataTaskHandle()`
  - Process queue in loop via `radio.getNextAppPacket()`
  - Identify packets containing CanarinMessage data
  - Queue received messages for MQTT publishing
  - Track source node address for routing info

  **Must NOT do**:
  - Process packets on radio interrupt (defer to loop)
  - Store unlimited messages (max 10 queue)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: Standard callback pattern
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 3, 5)
  - **Blocks**: Task 7
  - **Blocked By**: None

  **References**:
  - `firmware/production/src/main.cpp:450-500` - Packet receive callback pattern
  - `src/LoraMesher.h:236` - `setReceiveAppDataTaskHandle()` for receive notification
  - `firmware/production/src/main.cpp:processReceivedPackets()` - Queue-based receive pattern
  - `lib/xmesh-canarin-bridge/src/CanarinMessage.h` - Message format (from Task 2)

  **Acceptance Criteria**:
  - `GatewayReceiver::begin()` registers task handle and starts receive loop
  - [ ] `GatewayReceiver::available()` returns pending message count
  - [ ] `GatewayReceiver::pop()` returns next CanarinMessage
  - [ ] Queue drops oldest when full (log warning)

  **Commit**: YES
  - Message: `feat(canarin-bridge): add gateway receiver for mesh packets`
  - Files: `lib/xmesh-canarin-bridge/src/GatewayReceiver.*`
  - Pre-commit: `pio build`

---

- [ ] 7. MQTT Publisher with JSON Conversion

  **What to do**:
  - Create `lib/xmesh-canarin-bridge/src/MQTTPublisher.h/cpp`
  - Convert CanarinMessage to JSON format
  - Publish to topic: `xmesh/canarin/<source_addr>/sensors`
  - Include all sensor readings with proper field names
  - Add timestamp from message header

  **Must NOT do**:
  - Use raw binary format for MQTT
  - Implement MQTT client (use existing PubSubClient)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard JSON serialization
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Task 8)
  - **Blocks**: Task 9
  - **Blocked By**: Tasks 2, 6

  **References**:
  - `firmware/production/src/main.cpp:255-274` - Existing MQTT pattern
  - `.canarinv5-LEGO/components/can5_net/can5_protocols/can5_lorarelay/can5_loramsg.h:12-43` - Sensor field names
  - ArduinoJson library documentation

  **Acceptance Criteria**:
  - [ ] JSON output includes: timestamp, source_addr, sensors array
  - [ ] Each sensor has: type (string), value (number), unit (string)
  - [ ] Topic format: `xmesh/canarin/XXXX/sensors` where XXXX is hex address
  ```bash
  # Gateway test with mosquitto
  mosquitto_sub -h localhost -t "xmesh/canarin/+/sensors" -C 1 | jq '.sensors[0].type'
  # Assert: Output is a string like "pm2_5" or "temp"
  ```

  **Commit**: YES
  - Message: `feat(canarin-bridge): add MQTT publisher with JSON conversion`
  - Files: `lib/xmesh-canarin-bridge/src/MQTTPublisher.*`
  - Pre-commit: `pio build`

---

- [ ] 8. Integration Test with Hardware

  **What to do**:
  - Set up 2+ Heltec nodes: 1 bridge, 1 gateway
  - Connect bridge to serial terminal (simulating Canarin)
  - Inject AT+SEND commands via serial
  - Verify mesh packet received at gateway
  - Verify MQTT JSON output

  **Must NOT do**:
  - Require actual Canarin hardware (serial simulation OK)
  - Skip any test steps

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Multi-device integration testing
  - **Skills**: [`playwright`, `git-master`]
    - `playwright`: For any browser-based MQTT visualization

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Task 7)
  - **Blocks**: Task 10
  - **Blocked By**: Tasks 3, 5

  **References**:
  - `firmware/production/src/main.cpp` - Full firmware
  - `mqtt-setup/docker-compose.yml` - Test MQTT broker

  **Acceptance Criteria**:
  - [ ] Flash bridge firmware to Node A
  - [ ] Flash gateway firmware to Node B
  - [ ] Send via serial to Node A: `AT+VER\r\n`
  - [ ] Assert: Response contains "xMESH"
  - [ ] Send via serial to Node A: `AT+SEND=21:0:800000006112345678\r\n`
  - [ ] Assert: Node B receives mesh packet
  - [ ] Assert: MQTT message published with JSON payload
  ```bash
  # Terminal 1: Start MQTT subscriber
  mosquitto_sub -h localhost -t "xmesh/#" -v
  
  # Terminal 2: Inject AT command to bridge node
  echo -e "AT+SEND=21:0:800000006112345678\r\n" > /dev/tty.usbserial-XXX
  
  # Assert: Terminal 1 shows JSON message with timestamp
  ```

  **Commit**: YES
  - Message: `test(canarin-bridge): add integration test documentation`
  - Files: `docs/canarin-bridge-integration-test.md` (if created)
  - Pre-commit: N/A (manual test)

---

- [ ] 9. TTN MQTT Bridge Documentation

  **What to do**:
  - Document how to connect gateway to TTN MQTT broker
  - Provide configuration for TTN MQTT integration
  - Explain payload format compatibility with TTN decoders
  - Optional: Create TTN payload decoder JavaScript

  **Must NOT do**:
  - Implement full LoRaWAN stack
  - Require TTN for basic operation

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation task
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 7, 8)
  - **Blocks**: None
  - **Blocked By**: Task 7

  **References**:
  - TTN MQTT integration documentation
  - `.canarinv5-LEGO/scripts/tools/sample_files/sample_provision_lorawan.json` - TTN config example

  **Acceptance Criteria**:
  - [ ] Document created: `docs/ttn-mqtt-bridge.md`
  - [ ] Includes: TTN MQTT broker address, topic structure
  - [ ] Includes: Sample mosquitto_bridge.conf
  - [ ] Includes: Payload decoder function (JavaScript)

  **Commit**: YES
  - Message: `docs(canarin-bridge): add TTN MQTT bridge guide`
  - Files: `docs/ttn-mqtt-bridge.md`
  - Pre-commit: N/A

---

### Phase 2: New Binary Protocol (Optional Optimization)

- [ ] 10. Design and Implement Optimized Binary Protocol

  **What to do**:
  - Design efficient binary frame format (no hex encoding)
  - Create `can5_netif_mesh` driver for Canarin firmware
  - Implement bidirectional communication (ACKs, config)
  - Benchmark: compare latency and throughput vs AT emulation

  **Must NOT do**:
  - Break AT emulation compatibility
  - Force migration (AT mode remains available)

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain`
    - Reason: Protocol design, dual-firmware coordination
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (Phase 2)
  - **Blocks**: None
  - **Blocked By**: Task 8

  **References**:
  - `.canarinv5-LEGO/components/can5_net/include/can5_netif.h` - Network interface pattern
  - `.canarinv5-LEGO/components/can5_net/can5_netif/can5_netif_lwan/` - Existing driver to copy
  - `lib/xmesh-canarin-bridge/` - Heltec-side implementation

  **Acceptance Criteria**:
  - [ ] Binary frame format documented
  - [ ] Canarin driver compiles with ESP-IDF 4.4
  - [ ] Heltec handler decodes binary frames
  - [ ] Benchmark shows >30% efficiency improvement over AT mode
  - [ ] Both modes selectable at runtime

  **Commit**: YES (multiple commits)
  - Message: `feat(canarin-bridge): add optimized binary protocol (Phase 2)`
  - Files: Multiple across xMESH and LEGO repos
  - Pre-commit: `pio test -e native`

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `feat(canarin-bridge): add AT command parser` | ATCommandParser.*, test_at_parser.cpp | pio test |
| 2 | `feat(canarin-bridge): add CanarinMessage decoder` | CanarinMessage.*, test_canarin_message.cpp | pio test |
| 3 | `feat(canarin-bridge): integrate mesh TX` | CanarinBridge.* | pio build |
| 4 | `feat(canarin-bridge): add UART handler` | UARTHandler.* | pio build |
| 5 | `feat(config): add bridge mode` | platformio.ini, config.h, main.cpp | pio run -e bridge |
| 6 | `feat(canarin-bridge): add gateway receiver` | GatewayReceiver.* | pio build |
| 7 | `feat(canarin-bridge): add MQTT publisher` | MQTTPublisher.* | pio build |
| 8 | `test(canarin-bridge): integration test` | docs/*.md | manual |
| 9 | `docs(canarin-bridge): TTN bridge guide` | docs/ttn-mqtt-bridge.md | N/A |
| 10 | `feat(canarin-bridge): binary protocol` | Multiple | pio test |

---

## Success Criteria

### Verification Commands
```bash
# Build all configurations
pio run                    # Expected: SUCCESS
pio run -e bridge          # Expected: SUCCESS
pio test -e native         # Expected: All tests pass

# MQTT test (with broker running)
mosquitto_sub -h localhost -t "xmesh/canarin/+/sensors" -C 1 | jq '.sensors | length'
# Expected: Integer > 0
```

### Final Checklist
- [ ] All "Must Have" features implemented
- [ ] All "Must NOT Have" exclusions respected
- [ ] All unit tests pass
- [ ] Integration test with 2+ nodes successful
- [ ] Documentation complete
- [ ] No Canarin firmware changes (Phase 1)
