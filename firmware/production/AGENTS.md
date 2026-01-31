# firmware/production - Main Entry Point

## PURPOSE

Production firmware for Heltec WiFi LoRa 32 V3. Integrates all xMESH modules.

## BUILD

```bash
pio run                                    # Build
pio run -t upload                          # Flash USB
pio run -e ota -t upload --upload-port IP  # OTA
pio test -e native                         # Unit tests
```

## KEY FILES

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, integrates all modules (~1300 lines) |
| `include/config.h` | All tunables (Trickle, cost weights, pins) |
| `include/DutyCycleBudget.h` | Regulatory duty cycle tracking |
| `platformio.ini` | Build config, dependencies |
| `test/test_native/` | Unity test suite |

## RUNTIME CONFIG (NVS)

Persisted in ESP32 NVS:
- `is_gateway` - Gateway role flag
- `wifi_ssid`, `wifi_pass` - WiFi credentials
- `mqtt_broker`, `mqtt_port` - MQTT server
- `mobility_en` - Mobility detection toggle

## FEATURES

- **Sensors:** PMS7003 (PM2.5), GPS (lat/lon/alt)
- **Display:** SSD1306 OLED status
- **MQTT:** Gateway forwards SensorPackets to broker
- **OTA:** ArduinoOTA or HTTP update
- **Security:** Optional AES encryption

## CALLBACKS FLOW

1. `helloReceivedCallback(srcAddr)` - on HELLO packet
2. Updates Trickle, ETX, GatewayBalancer, MobilityDetector
3. `costCalculationCallback(hops, via, destAddr)` - on route decision
4. Returns cost from CostRouter with ETX and gateway bias

## TESTS

| Test | Coverage |
|------|----------|
| `test_cost_router` | Normalization, cost formula |
| `test_etx_tracker` | Sequence gaps, EWMA |
| `test_trickle_scheduler` | Suppression, interval doubling |
| `test_gateway_balancer` | Load encoding, failure detection |
| `test_security_manager` | Encrypt/decrypt, replay |
