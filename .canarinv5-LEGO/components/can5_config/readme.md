# Congifuration Manager
The [Configuration Manager]() exposes configuration to the user through an HTTP server accessed over the wifi.

To save power, the Configuration Manager can be set to on or off.

## Ports
There are total `P1-P4`(4 ADC), `P6-P13`(8 I2C) and `P14-P21`(8 UART) ports available in the Multiplixer board.
The control board has `NETPORT_1` (1 UART) for attaching LoRaWAN or 4g module.

Presently the sensors are:
```
0. None
1. PM       (UART)
2. CO2      (UART)
3. GPS      (UART)
4. CO       (ADC)
5. BME680   (I2C)
6. BME280   (I2C)
```

Consult [can5_config_types.h](include/can5_config_types.h) for the key values used in [fields.json](src/fields.json). 


## Tests
To build the tests, select "Build CONGIF Tests" in the SDK configurator