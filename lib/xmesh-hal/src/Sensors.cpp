#include "xmesh/hal/Sensors.h"
#include <PMserial.h>
#include <TinyGPSPlus.h>
#include <esp_log.h>

static const char* TAG = "Sensors";

namespace xmesh {
namespace hal {

bool Sensors::beginAirQuality(HardwareSerial* serial) {
    pms_ = new SerialPM(PMS7003, *serial);
    pms_->init();
    ESP_LOGI(TAG, "PMS7003 initialized");
    return true;
}

bool Sensors::beginGPS(HardwareSerial* serial) {
    gps_ = new TinyGPSPlus();
    gps_serial_ = serial;
    ESP_LOGI(TAG, "GPS initialized");
    return true;
}

AirQualityData Sensors::readAirQuality() {
    AirQualityData data = {};
    data.valid = false;
    
    if (pms_ == nullptr) return data;
    
    pms_->read();
    if (*pms_) {  // Check if read was successful
        data.pm1_0 = pms_->pm01;
        data.pm2_5 = pms_->pm25;
        data.pm10 = pms_->pm10;
        data.timestamp_ms = millis();
        data.valid = true;
    }
    return data;
}

GPSData Sensors::readGPS() {
    GPSData data = {};
    data.valid = false;
    
    if (gps_ == nullptr || gps_serial_ == nullptr) return data;
    
    // Non-blocking: process available characters
    while (gps_serial_->available() > 0) {
        gps_->encode(gps_serial_->read());
    }
    
    if (gps_->location.isValid() && gps_->location.age() < 2000) {
        data.latitude = gps_->location.lat();
        data.longitude = gps_->location.lng();
        data.altitude = gps_->altitude.isValid() ? gps_->altitude.meters() : 0.0f;
        data.satellites = gps_->satellites.isValid() ? gps_->satellites.value() : 0;
        data.timestamp_ms = millis();
        data.valid = true;
    }
    return data;
}

void Sensors::update() {
    // Called from main loop - process GPS data non-blockingly
    if (gps_serial_ != nullptr && gps_ != nullptr) {
        while (gps_serial_->available() > 0) {
            gps_->encode(gps_serial_->read());
        }
    }
}

} // namespace hal
} // namespace xmesh
