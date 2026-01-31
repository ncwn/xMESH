#ifndef XMESH_HAL_SENSORS_H
#define XMESH_HAL_SENSORS_H

#include <Arduino.h>
#include <xmesh/hal/SensorPacket.h>

class SerialPM;
class TinyGPSPlus;

namespace xmesh {
namespace hal {

struct AirQualityData {
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm10;
    uint32_t timestamp_ms;
    bool valid;
};

struct GPSData {
    double latitude;
    double longitude;
    float altitude;
    uint8_t satellites;
    uint32_t timestamp_ms;
    bool valid;
};

enum class PMSState : uint8_t {
    OFF,
    SLEEPING,
    WARMING,
    READY
};

class Sensors {
public:
    bool beginAirQuality(HardwareSerial* serial);
    bool beginGPS(HardwareSerial* serial);
    ~Sensors();

    AirQualityData readAirQuality();
    GPSData readGPS();

    void update();

    bool detectPMS(uint32_t timeoutMs = 3000);
    bool detectGPS(uint32_t timeoutMs = 2000);

    void setPMSPower(bool on);
    PMSState getPMSState() const;

    bool isPMSDetected() const;
    bool isGPSDetected() const;
    NodeMode getNodeMode() const;

    void updatePowerState();

    void setPMSSetPin(uint8_t pin);
    void setWarmupMs(uint32_t ms);
    void setReadIntervalMs(uint32_t ms);

private:
    SerialPM* pms_;
    TinyGPSPlus* gps_;
    HardwareSerial* gps_serial_;
    HardwareSerial* pms_serial_;

    bool pms_detected_ = false;
    bool gps_detected_ = false;
    bool pms_power_on_ = false;

    PMSState pms_state_ = PMSState::OFF;
    uint32_t warmup_start_ms_ = 0;
    uint32_t last_read_ms_ = 0;

    uint8_t pms_set_pin_ = 3;
    uint32_t pms_warmup_ms_ = 30000;
    uint32_t read_interval_ms_ = 60000;
};

} // namespace hal
} // namespace xmesh

#endif // XMESH_HAL_SENSORS_H
