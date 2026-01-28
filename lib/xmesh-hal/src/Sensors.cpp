#include "hal/Sensors.h"

namespace xmesh {
namespace hal {

bool Sensors::beginAirQuality(HardwareSerial* serial) {
    pms_impl_ = nullptr;
    return false;
}

bool Sensors::beginGPS(HardwareSerial* serial) {
    gps_impl_ = nullptr;
    return false;
}

AirQualityData Sensors::readAirQuality() {
    AirQualityData data = {0};
    data.valid = false;
    return data;
}

GPSData Sensors::readGPS() {
    GPSData data = {0};
    data.valid = false;
    return data;
}

void Sensors::update() {
}

} // namespace hal
} // namespace xmesh
