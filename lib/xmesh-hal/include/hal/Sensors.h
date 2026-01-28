#ifndef XMESH_HAL_SENSORS_H
#define XMESH_HAL_SENSORS_H

#include <Arduino.h>

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

class Sensors {
public:
    bool beginAirQuality(HardwareSerial* serial);
    bool beginGPS(HardwareSerial* serial);

    AirQualityData readAirQuality();
    GPSData readGPS();

    void update();

private:
    void* pms_impl_;
    void* gps_impl_;
};

} // namespace hal
} // namespace xmesh

#endif // XMESH_HAL_SENSORS_H
