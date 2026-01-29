#include "xmesh/hal/Sensors.h"
#include <PMserial.h>
#include <TinyGPSPlus.h>
#include <esp_log.h>

static const char* TAG = "Sensors";

namespace xmesh {
namespace hal {

bool Sensors::beginAirQuality(HardwareSerial* serial) {
    pms_ = new SerialPM(PMS7003, *serial);
    pms_serial_ = serial;
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
    if (*pms_) {
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
    if (gps_serial_ != nullptr && gps_ != nullptr) {
        while (gps_serial_->available() > 0) {
            gps_->encode(gps_serial_->read());
        }
    }
}

bool Sensors::detectPMS(uint32_t timeoutMs) {
    if (pms_ == nullptr || pms_serial_ == nullptr) {
        pms_detected_ = false;
        return false;
    }
    
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        pms_->read();
        if (*pms_) {
            pms_detected_ = true;
            ESP_LOGI(TAG, "PMS7003 detected (valid data in %lu ms)", millis() - start);
            return true;
        }
        delay(100);
    }
    
    pms_detected_ = false;
    ESP_LOGW(TAG, "PMS7003 not detected (timeout %lu ms)", timeoutMs);
    return false;
}

bool Sensors::detectGPS(uint32_t timeoutMs) {
    if (gps_ == nullptr || gps_serial_ == nullptr) {
        gps_detected_ = false;
        return false;
    }
    
    uint32_t start = millis();
    uint32_t chars_received = 0;
    
    while (millis() - start < timeoutMs) {
        while (gps_serial_->available() > 0) {
            char c = gps_serial_->read();
            gps_->encode(c);
            chars_received++;
        }
        
        if (chars_received > 50) {
            gps_detected_ = true;
            ESP_LOGI(TAG, "GPS detected (%lu chars in %lu ms)", chars_received, millis() - start);
            return true;
        }
        delay(50);
    }
    
    gps_detected_ = (chars_received > 0);
    if (gps_detected_) {
        ESP_LOGI(TAG, "GPS detected (partial: %lu chars)", chars_received);
    } else {
        ESP_LOGW(TAG, "GPS not detected (timeout %lu ms)", timeoutMs);
    }
    return gps_detected_;
}

void Sensors::setPMSPower(bool on) {
    if (pms_set_pin_ == 0) return;
    
    pinMode(pms_set_pin_, OUTPUT);
    digitalWrite(pms_set_pin_, on ? HIGH : LOW);
    pms_power_on_ = on;
    
    if (on) {
        pms_state_ = PMSState::WARMING;
        warmup_start_ms_ = millis();
        ESP_LOGI(TAG, "PMS SET HIGH - warming up");
    } else {
        pms_state_ = PMSState::SLEEPING;
        ESP_LOGI(TAG, "PMS SET LOW - sleeping");
    }
}

bool Sensors::getPMSPower() const {
    return pms_power_on_;
}

PMSState Sensors::getPMSState() const {
    return pms_state_;
}

bool Sensors::isPMSDetected() const {
    return pms_detected_;
}

bool Sensors::isGPSDetected() const {
    return gps_detected_;
}

NodeMode Sensors::getNodeMode() const {
    if (pms_detected_ || gps_detected_) {
        return NodeMode::SENSOR;
    }
    return NodeMode::RELAY;
}

void Sensors::updatePowerState() {
    if (!pms_detected_) return;
    
    uint32_t now = millis();
    
    switch (pms_state_) {
    case PMSState::OFF:
        break;
        
    case PMSState::SLEEPING:
        if (now - last_read_ms_ >= read_interval_ms_ - pms_warmup_ms_) {
            setPMSPower(true);
        }
        break;
        
    case PMSState::WARMING:
        if (now - warmup_start_ms_ >= pms_warmup_ms_) {
            pms_state_ = PMSState::READY;
            ESP_LOGI(TAG, "PMS warmup complete - ready");
        }
        break;
        
    case PMSState::READY:
        break;
    }
}

void Sensors::setPMSSetPin(uint8_t pin) {
    pms_set_pin_ = pin;
}

void Sensors::setWarmupMs(uint32_t ms) {
    pms_warmup_ms_ = ms;
}

void Sensors::setReadIntervalMs(uint32_t ms) {
    read_interval_ms_ = ms;
}

} // namespace hal
} // namespace xmesh
