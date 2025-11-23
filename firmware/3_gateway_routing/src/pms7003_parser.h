/**
 * @file pms7003_parser.h
 * @brief Parser for PMS7003 Particulate Matter Sensor
 *
 * Reads and parses data from Plantower PMS7003 sensor via UART.
 * Protocol: 9600 baud, 8N1, auto-transmission mode
 * Frame: 32 bytes total, starts with 0x42 0x4D
 *
 * Data Format:
 * - PM1.0, PM2.5, PM10 (standard particles, atmospheric environment)
 * - Particle counts for different sizes
 * - Checksum validation
 */

#ifndef PMS7003_PARSER_H
#define PMS7003_PARSER_H

#include <Arduino.h>

// PMS7003 Frame Structure
#define PMS_FRAME_START1    0x42
#define PMS_FRAME_START2    0x4D
#define PMS_FRAME_LENGTH    32
#define PMS_DATA_LENGTH     28  // Excluding start bytes and checksum

/**
 * @brief PMS7003 Data Structure
 */
struct PMS7003Data {
    // Concentration Units (µg/m³)
    uint16_t pm1_0_standard;    // PM1.0 concentration (CF=1, standard particle)
    uint16_t pm2_5_standard;    // PM2.5 concentration (CF=1, standard particle)
    uint16_t pm10_standard;     // PM10 concentration (CF=1, standard particle)

    uint16_t pm1_0_atmospheric; // PM1.0 concentration (atmospheric environment)
    uint16_t pm2_5_atmospheric; // PM2.5 concentration (atmospheric environment)
    uint16_t pm10_atmospheric;  // PM10 concentration (atmospheric environment)

    // Particle counts (per 0.1L air)
    uint16_t particles_0_3um;
    uint16_t particles_0_5um;
    uint16_t particles_1_0um;
    uint16_t particles_2_5um;
    uint16_t particles_5_0um;
    uint16_t particles_10um;

    // Status
    uint8_t version;
    uint8_t error_code;
    bool valid;
    uint32_t last_update;
};

/**
 * @brief PMS7003 Sensor Parser Class
 */
class PMS7003Parser {
private:
    HardwareSerial* serial;
    PMS7003Data data;
    uint8_t buffer[PMS_FRAME_LENGTH];
    uint8_t buffer_index;
    bool frame_started;
    uint32_t last_valid_read;
    uint32_t read_count;
    uint32_t error_count;

    /**
     * @brief Calculate checksum for frame validation
     */
    uint16_t calculateChecksum(uint8_t* buf, uint8_t length) {
        uint16_t sum = 0;
        for (uint8_t i = 0; i < length; i++) {
            sum += buf[i];
        }
        return sum;
    }

    /**
     * @brief Parse complete frame
     */
    bool parseFrame() {
        // Verify start bytes
        if (buffer[0] != PMS_FRAME_START1 || buffer[1] != PMS_FRAME_START2) {
            return false;
        }

        // Extract frame length (bytes 2-3, big-endian)
        uint16_t frame_len = (buffer[2] << 8) | buffer[3];
        if (frame_len != PMS_DATA_LENGTH) {
            return false;
        }

        // Verify checksum (last 2 bytes)
        uint16_t calc_checksum = calculateChecksum(buffer, PMS_FRAME_LENGTH - 2);
        uint16_t recv_checksum = (buffer[30] << 8) | buffer[31];

        if (calc_checksum != recv_checksum) {
            error_count++;
            return false;
        }

        // Parse data (all values are big-endian uint16_t)
        data.pm1_0_standard = (buffer[4] << 8) | buffer[5];
        data.pm2_5_standard = (buffer[6] << 8) | buffer[7];
        data.pm10_standard = (buffer[8] << 8) | buffer[9];

        data.pm1_0_atmospheric = (buffer[10] << 8) | buffer[11];
        data.pm2_5_atmospheric = (buffer[12] << 8) | buffer[13];
        data.pm10_atmospheric = (buffer[14] << 8) | buffer[15];

        data.particles_0_3um = (buffer[16] << 8) | buffer[17];
        data.particles_0_5um = (buffer[18] << 8) | buffer[19];
        data.particles_1_0um = (buffer[20] << 8) | buffer[21];
        data.particles_2_5um = (buffer[22] << 8) | buffer[23];
        data.particles_5_0um = (buffer[24] << 8) | buffer[25];
        data.particles_10um = (buffer[26] << 8) | buffer[27];

        data.version = buffer[28];
        data.error_code = buffer[29];

        data.valid = true;
        data.last_update = millis();
        last_valid_read = millis();
        read_count++;

        return true;
    }

public:
    /**
     * @brief Constructor
     * @param ser Pointer to HardwareSerial object
     */
    PMS7003Parser(HardwareSerial* ser) : serial(ser) {
        buffer_index = 0;
        frame_started = false;
        last_valid_read = 0;
        read_count = 0;
        error_count = 0;
        data.valid = false;
    }

    /**
     * @brief Initialize PMS7003 sensor
     * @param rx_pin GPIO pin for RX (sensor TX → ESP32)
     * @param tx_pin GPIO pin for TX (ESP32 → sensor)
     */
    void begin(int rx_pin, int tx_pin) {
        serial->begin(PMS_BAUD, SERIAL_8N1, rx_pin, tx_pin);
        Serial.printf("[PMS] Initialized on RX=%d, TX=%d, baud=%d\n", rx_pin, tx_pin, PMS_BAUD);

        // Wait for sensor warmup (PMS7003 needs ~30 seconds for stable readings)
        delay(1000);

        // Flush any partial data
        while (serial->available()) {
            serial->read();
        }

        Serial.println("[PMS] Ready");
    }

    /**
     * @brief Update parser (call in loop)
     * @return true if new valid frame parsed
     */
    bool update() {
        bool new_data = false;

        while (serial->available()) {
            uint8_t byte = serial->read();

            // Look for frame start
            if (!frame_started) {
                if (buffer_index == 0 && byte == PMS_FRAME_START1) {
                    buffer[buffer_index++] = byte;
                } else if (buffer_index == 1 && byte == PMS_FRAME_START2) {
                    buffer[buffer_index++] = byte;
                    frame_started = true;
                } else {
                    buffer_index = 0;
                }
            } else {
                // Collect frame bytes
                buffer[buffer_index++] = byte;

                // Frame complete?
                if (buffer_index >= PMS_FRAME_LENGTH) {
                    if (parseFrame()) {
                        new_data = true;
                    }

                    // Reset for next frame
                    buffer_index = 0;
                    frame_started = false;
                }
            }
        }

        return new_data;
    }

    /**
     * @brief Get latest sensor data
     */
    PMS7003Data getData() {
        return data;
    }

    /**
     * @brief Check if data is valid and recent
     * @param max_age_ms Maximum age in milliseconds
     */
    bool isDataValid(uint32_t max_age_ms = 5000) {
        if (!data.valid) return false;
        return (millis() - data.last_update) < max_age_ms;
    }

    /**
     * @brief Get read statistics
     */
    void getStats(uint32_t& reads, uint32_t& errors) {
        reads = read_count;
        errors = error_count;
    }

    /**
     * @brief Print data to serial (for debugging)
     */
    void printData() {
        if (!data.valid) {
            Serial.println("[PMS] No valid data");
            return;
        }

        uint32_t age = millis() - data.last_update;
        Serial.printf("[PMS] PM1.0=%d PM2.5=%d PM10=%d µg/m³ (age=%lums)\n",
                     data.pm1_0_atmospheric,
                     data.pm2_5_atmospheric,
                     data.pm10_atmospheric,
                     age);
    }
};

#endif // PMS7003_PARSER_H
