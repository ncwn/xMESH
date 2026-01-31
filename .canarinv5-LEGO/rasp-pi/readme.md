# Read Canarin

This application monitors data from the UART port of Canarin sensors and stores real-time sensor readings in an SQLite database.

### Database Format

| timestamp     | sensor_name            | port   | type | value_s    |
|---------------|------------------------|--------|------|------------|
| 1759301347    | UBLOX_NEO_GPS_LAT      | uart6  | d    | 14.077493  |
| 1759301347    | UBLOX_NEO_GPS_LNG      | uart6  | d    | 100.613156 |
| 1759301347    | UBLOX_NEO_GPS_ALT      | uart6  | d    | 23.703000  |
| 1759301347    | UBLOX_NEO_IMU_NORTH    | uart6  | d    | 0.013000   |
| 1759301347    | UBLOX_NEO_IMU_EAST     | uart6  | d    | 0.005000   |
| 1759301347    | UBLOX_NEO_IMU_DOWN     | uart6  | d    | 0.051000   |
| 1759301347    | PMS7003_PM1_0_CF1      | uart1  | d    | 6.000000   |
| 1759301347    | PMS7003_PM2_5_CF1      | uart1  | d    | 7.000000   |
| 1759301347    | PMS7003_PM10_CF1       | uart1  | d    | 7.000000   |
| 1759301347    | PMS7003_PM1_0_ATM      | uart1  | d    | 6.000000   |
| 1759301347    | PMS7003_PM2_5_ATM      | uart1  | d    | 7.000000   |
| 1759301347    | PMS7003_PM10_ATM       | uart1  | d    | 7.000000   |
| 1759301347    | ZE07_CO                | uart7  | d    | 0.000000   |

* **timestamp**: Epoch time (UTC).
* **sensor_name**: Name of the sensor module.
* **port**: Port to which the sensor is connected.
* **type**: Data type: n (numeric), d (decimal), or s (string).
* **value_s**: Sensor value formatted as a string.

### Supported Sensor Modules

The following sensor modules are supported:

- UBLOX_NEO_GPS_LAT
- UBLOX_NEO_GPS_LNG
- UBLOX_NEO_GPS_ALT
- UBLOX_NEO_IMU_NORTH
- UBLOX_NEO_IMU_EAST
- UBLOX_NEO_IMU_DOWN
- PMS7003_PM1_0_CF1
- PMS7003_PM2_5_CF1
- PMS7003_PM10_CF1
- PMS7003_PM1_0_ATM
- PMS7003_PM2_5_ATM
- PMS7003_PM10_ATM
- MH_Z16_CO2
- ZE03_NO2
- ZE07_CO
- BME280_TEMP
- BME280_PRES
- BME280_HUMI
- WS3226_RAIN
- WS3226_WIND_SPD
- WS3226_WIND_DIR
- WS3226_BATTERY_VOLT
- WS3226_BATTERY_PERCENTAGE
- SIM7600_GPS_LAT
- SIM7600_GPS_LNG
- SIM7600_GPS_ALT
- MPU6400_ACCEL_X
- MPU6400_ACCEL_Y
- MPU6400_ACCEL_Z
- MPU6400_GYRO_X
- MPU6400_GYRO_Y
- MPU6400_GYRO_Z
- SCD41_CO2
- SCD41_HUMI
- SCD41_TEMP


## Syslog To /var/log With Rotation

This app logs to syslog. 


To Follow live logs by tag: `sudo journalctl -t read_canarin -f`


For more long-term logging on Ubuntu, use rsyslog + logrotate to write/rotate a dedicated log file.
  Steps (root):

  1) Install config to route logs to `/var/log/read_canarin.log`:
      - Copy `rasp-pi/rsyslog/30-read_canarin.conf` to `/etc/rsyslog.d/30-read_canarin.conf`.
      - Validate and restart rsyslog:
        - `sudo rsyslogd -N1`
        - `sudo systemctl restart rsyslog`
      - Tail the file to verify:
        - `sudo tail -f /var/log/read_canarin.log`

  2) Install log rotation policy:
      - Copy `rasp-pi/logrotate/read_canarin` to `/etc/logrotate.d/read_canarin`.
      - Dry-run test:
        - `sudo logrotate -d /etc/logrotate.d/read_canarin`

Notes:
- The logger name is `read_canarin`; messages look like: `read_canarin[PID]: LEVEL: ...`.
- The code attempts to set a syslog tag (ident) to `read_canarin:`; if your Python version/handler ignores it, the rsyslog config includes a fallback match by message prefix.
- No app write access to `/var/log` is required; rsyslog creates/rotates the file.
