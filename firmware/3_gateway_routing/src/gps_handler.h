/**
 * @file gps_handler.h
 * @brief GPS handler for NEO-M8M-0-10 GPS module
 *
 * Uses TinyGPS++ library to parse NMEA sentences from u-blox NEO-M8M GPS module.
 * Provides latitude, longitude, altitude, satellite count, and fix validity.
 */

#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <Arduino.h>
#include <TinyGPSPlus.h>

/**
 * @brief GPS Data Structure
 */
struct GPSData {
    double latitude;        // Decimal degrees (-90 to +90)
    double longitude;       // Decimal degrees (-180 to +180)
    float altitude;         // Meters above sea level
    uint8_t satellites;     // Number of satellites in use
    float hdop;             // Horizontal Dilution of Precision
    uint8_t hour, minute, second;
    uint16_t year;
    uint8_t month, day;
    bool valid;             // GPS fix valid
    uint32_t last_update;   // Timestamp of last valid fix
};

/**
 * @brief GPS Handler Class using TinyGPS++
 */
class GPSHandler {
private:
    TinyGPSPlus gps;
    HardwareSerial* serial;
    GPSData data;
    uint32_t chars_processed;
    uint32_t sentences_with_fix;
    uint32_t failed_checksum;
    uint32_t last_encode_time;

public:
    /**
     * @brief Constructor
     * @param ser Pointer to HardwareSerial object
     */
    GPSHandler(HardwareSerial* ser) : serial(ser) {
        chars_processed = 0;
        sentences_with_fix = 0;
        failed_checksum = 0;
        last_encode_time = 0;
        data.valid = false;
        data.last_update = 0;
    }

    /**
     * @brief Initialize GPS module
     * @param rx_pin GPIO pin for RX (GPS TX → ESP32)
     * @param tx_pin GPIO pin for TX (ESP32 → GPS)
     */
    void begin(int rx_pin, int tx_pin) {
        serial->begin(GPS_BAUD, SERIAL_8N1, rx_pin, tx_pin);
        Serial.printf("[GPS] Initialized on RX=%d, TX=%d, baud=%d\n", rx_pin, tx_pin, GPS_BAUD);

        // Flush any initial data
        delay(500);
        while (serial->available()) {
            serial->read();
        }

        Serial.println("[GPS] Waiting for satellite fix...");
        Serial.println("[GPS] Note: May take 1-5 minutes outdoors, longer indoors");
    }

    /**
     * @brief Update GPS parser (call in loop)
     * @return true if new valid fix obtained
     */
    bool update() {
        bool new_fix = false;
        uint32_t start_time = millis();

        // Feed GPS parser with available UART data
        while (serial->available() && (millis() - start_time < 100)) {
            char c = serial->read();
            chars_processed++;

            if (gps.encode(c)) {
                last_encode_time = millis();

                // Check if we have a valid location fix
                if (gps.location.isValid()) {
                    data.latitude = gps.location.lat();
                    data.longitude = gps.location.lng();
                    data.altitude = gps.altitude.meters();
                    data.satellites = gps.satellites.value();
                    data.hdop = gps.hdop.hdop();

                    // Extract date/time if available
                    if (gps.date.isValid()) {
                        data.year = gps.date.year();
                        data.month = gps.date.month();
                        data.day = gps.date.day();
                    }

                    if (gps.time.isValid()) {
                        data.hour = gps.time.hour();
                        data.minute = gps.time.minute();
                        data.second = gps.time.second();
                    }

                    data.valid = true;
                    data.last_update = millis();
                    sentences_with_fix++;
                    new_fix = true;
                }
            }
        }

        // Track failed checksums
        if (gps.failedChecksum() > failed_checksum) {
            failed_checksum = gps.failedChecksum();
        }

        return new_fix;
    }

    /**
     * @brief Get latest GPS data
     */
    GPSData getData() {
        return data;
    }

    /**
     * @brief Check if GPS fix is valid and recent
     * @param max_age_ms Maximum age in milliseconds
     */
    bool isFixValid(uint32_t max_age_ms = 10000) {
        if (!data.valid) return false;
        return (millis() - data.last_update) < max_age_ms;
    }

    /**
     * @brief Get GPS fix age in milliseconds
     */
    uint32_t getFixAge() {
        if (!data.valid) return UINT32_MAX;
        return millis() - data.last_update;
    }

    /**
     * @brief Print GPS status to serial (for debugging)
     */
    void printStatus() {
        Serial.printf("[GPS] Chars: %lu, Sentences: %lu, Failed: %lu\n",
                     chars_processed, sentences_with_fix, failed_checksum);

        if (data.valid) {
            uint32_t age = millis() - data.last_update;
            Serial.printf("[GPS] Lat: %.6f°, Lon: %.6f°, Alt: %.1fm\n",
                         data.latitude, data.longitude, data.altitude);
            Serial.printf("[GPS] Sats: %d, HDOP: %.2f, Age: %lums\n",
                         data.satellites, data.hdop, age);

            if (gps.date.isValid() && gps.time.isValid()) {
                Serial.printf("[GPS] Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                             data.year, data.month, data.day,
                             data.hour, data.minute, data.second);
            }
        } else {
            Serial.println("[GPS] No fix yet (move to window/outdoors for better signal)");
        }
    }

    /**
     * @brief Print data to serial (compact format)
     */
    void printData() {
        if (!data.valid) {
            Serial.println("[GPS] No valid fix");
            return;
        }

        uint32_t age = millis() - data.last_update;
        Serial.printf("[GPS] %.6f°N, %.6f°E, %d sats, alt=%.1fm (age=%lums)\n",
                     data.latitude, data.longitude,
                     data.satellites, data.altitude, age);
    }

    /**
     * @brief Get TinyGPS++ object (for advanced usage)
     */
    TinyGPSPlus& getGPS() {
        return gps;
    }
};

#endif // GPS_HANDLER_H
