/**
 * @file sensor_data.h
 * @brief Enhanced sensor data structure for PM + GPS transmission via LoRa
 *
 * Compact structure for transmitting:
 * - Particulate Matter (PM1.0, PM2.5, PM10) from PMS7003
 * - GPS coordinates (Lat, Lon, Alt, Satellites) from NEO-M8M
 * - Metadata (timestamp, sequence number)
 *
 * Total payload size: ~26 bytes (optimized for LoRa transmission)
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

/**
 * @brief Enhanced Sensor Data Packet
 *
 * Packed structure to minimize LoRa airtime.
 * Total size: 26 bytes
 */
struct __attribute__((packed)) EnhancedSensorData {
    // PM Sensor Data (6 bytes)
    uint16_t pm1_0;         // PM1.0 concentration (µg/m³, 0-65535)
    uint16_t pm2_5;         // PM2.5 concentration (µg/m³, 0-65535)
    uint16_t pm10;          // PM10 concentration (µg/m³, 0-65535)

    // GPS Data (12 bytes) - using float for compact representation
    float latitude;         // Decimal degrees (-90 to +90)
    float longitude;        // Decimal degrees (-180 to +180)
    float altitude;         // Meters above sea level (-1000 to +10000)

    // GPS Quality Indicators (2 bytes)
    uint8_t satellites;     // Number of satellites (0-255)
    uint8_t gps_valid;      // GPS fix status: 0=no fix, 1=valid fix

    // Metadata (6 bytes)
    uint32_t timestamp;     // Milliseconds since boot
    uint16_t sequence;      // Packet sequence number

    // Total: 26 bytes
};

/**
 * @brief Sensor Data Manager
 *
 * Helper functions for packing/unpacking sensor data
 */
class SensorDataManager {
public:
    /**
     * @brief Create enhanced sensor data packet from PM and GPS readings
     */
    static EnhancedSensorData createPacket(
        uint16_t pm1_0, uint16_t pm2_5, uint16_t pm10,
        double lat, double lon, float alt,
        uint8_t sats, bool gps_valid,
        uint32_t timestamp, uint16_t sequence)
    {
        EnhancedSensorData packet;

        // PM data
        packet.pm1_0 = pm1_0;
        packet.pm2_5 = pm2_5;
        packet.pm10 = pm10;

        // GPS data (convert double to float for compact storage)
        packet.latitude = (float)lat;
        packet.longitude = (float)lon;
        packet.altitude = alt;
        packet.satellites = sats;
        packet.gps_valid = gps_valid ? 1 : 0;

        // Metadata
        packet.timestamp = timestamp;
        packet.sequence = sequence;

        return packet;
    }

    /**
     * @brief Print sensor data packet (for debugging)
     */
    static void printPacket(const EnhancedSensorData& packet) {
        Serial.println("[PACKET] Enhanced Sensor Data:");
        Serial.printf("  PM1.0: %d µg/m³\n", packet.pm1_0);
        Serial.printf("  PM2.5: %d µg/m³\n", packet.pm2_5);
        Serial.printf("  PM10: %d µg/m³\n", packet.pm10);

        Serial.printf("  GPS: %.6f°N, %.6f°E\n", packet.latitude, packet.longitude);
        Serial.printf("  Altitude: %.1f m\n", packet.altitude);
        Serial.printf("  Satellites: %d\n", packet.satellites);
        Serial.printf("  GPS Valid: %s\n", packet.gps_valid ? "YES" : "NO");

        Serial.printf("  Timestamp: %lu ms\n", packet.timestamp);
        Serial.printf("  Sequence: %d\n", packet.sequence);
        Serial.printf("  Size: %d bytes\n", sizeof(EnhancedSensorData));
    }

    /**
     * @brief Get Air Quality Index (AQI) category from PM2.5
     * @param pm2_5 PM2.5 concentration in µg/m³
     * @return AQI category string
     */
    static const char* getAQICategory(uint16_t pm2_5) {
        if (pm2_5 <= 12) return "Good";
        else if (pm2_5 <= 35) return "Moderate";
        else if (pm2_5 <= 55) return "Unhealthy (Sensitive)";
        else if (pm2_5 <= 150) return "Unhealthy";
        else if (pm2_5 <= 250) return "Very Unhealthy";
        else return "Hazardous";
    }

    /**
     * @brief Get GPS fix quality description
     */
    static const char* getGPSQuality(uint8_t satellites, bool valid) {
        if (!valid) return "No Fix";
        if (satellites >= 8) return "Excellent";
        if (satellites >= 6) return "Good";
        if (satellites >= 4) return "Fair";
        return "Poor";
    }

    /**
     * @brief Validate packet data ranges
     */
    static bool validatePacket(const EnhancedSensorData& packet) {
        // Check PM values are reasonable (0-1000 µg/m³ is typical max)
        if (packet.pm1_0 > 1000 || packet.pm2_5 > 1000 || packet.pm10 > 1000) {
            return false;
        }

        // Check GPS coordinates are in valid ranges
        if (packet.latitude < -90.0 || packet.latitude > 90.0) {
            return false;
        }

        if (packet.longitude < -180.0 || packet.longitude > 180.0) {
            return false;
        }

        // Check altitude is reasonable (-500m to +10000m)
        if (packet.altitude < -500.0 || packet.altitude > 10000.0) {
            return false;
        }

        return true;
    }

    /**
     * @brief Serialize packet to byte array (for LoRa transmission)
     */
    static void serialize(const EnhancedSensorData& packet, uint8_t* buffer) {
        memcpy(buffer, &packet, sizeof(EnhancedSensorData));
    }

    /**
     * @brief Deserialize byte array to packet (at gateway)
     */
    static EnhancedSensorData deserialize(const uint8_t* buffer) {
        EnhancedSensorData packet;
        memcpy(&packet, buffer, sizeof(EnhancedSensorData));
        return packet;
    }

    /**
     * @brief Get packet size in bytes
     */
    static size_t getPacketSize() {
        return sizeof(EnhancedSensorData);
    }
};

#endif // SENSOR_DATA_H
