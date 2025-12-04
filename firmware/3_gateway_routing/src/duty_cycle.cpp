/**
 * @file duty_cycle.cpp
 * @brief Duty cycle monitoring and enforcement implementation
 */

#include "duty_cycle.h"
#include "logging.h"

// Global duty cycle monitor
DutyCycleMonitor dutyCycle;

DutyCycleMonitor::DutyCycleMonitor() {
    transmissions = nullptr;
    totalAirtimeMs = 0;
    windowStartMs = 0;
    enforcementEnabled = true;
    warningIssued = false;
    criticalWarningIssued = false;
    onWarningCallback = nullptr;
    onLimitCallback = nullptr;
    onResetCallback = nullptr;
}

DutyCycleMonitor::~DutyCycleMonitor() {
    // Free transmission records
    while (transmissions != nullptr) {
        TransmissionRecord* next = transmissions->next;
        delete transmissions;
        transmissions = next;
    }
}

void DutyCycleMonitor::begin(AirtimeConfig& cfg, bool enableEnforcement) {
    config = cfg;
    enforcementEnabled = enableEnforcement;
    windowStartMs = millis();
    reset();

    LOG_INFO("Duty cycle monitor initialized (SF%d, BW%.0f, CR4/%d)",
             config.spreadingFactor, config.bandwidth, config.codingRate);
}

void DutyCycleMonitor::setConfig(AirtimeConfig& cfg) {
    config = cfg;
}

bool DutyCycleMonitor::canTransmit(uint16_t packetSize) {
    updateWindow();

    unsigned long airtimeNeeded = calculateAirtime(packetSize);
    unsigned long projectedAirtime = totalAirtimeMs + airtimeNeeded;

    // Check if transmission would exceed limit
    if (projectedAirtime > DUTY_CYCLE_MAX_AIRTIME_MS) {
        if (enforcementEnabled) {
            LOG_WARN("Duty cycle limit would be exceeded: %lu/%lu ms",
                     projectedAirtime, DUTY_CYCLE_MAX_AIRTIME_MS);

            if (onLimitCallback) {
                float percentage = (projectedAirtime * 100.0) / DUTY_CYCLE_WINDOW_MS;
                onLimitCallback(percentage);
            }
            return false;
        } else {
            LOG_WARN("Duty cycle limit would be exceeded (not enforced): %lu/%lu ms",
                     projectedAirtime, DUTY_CYCLE_MAX_AIRTIME_MS);
        }
    }

    return true;
}

void DutyCycleMonitor::recordTransmission(uint16_t packetSize) {
    unsigned long airtimeMs = calculateAirtime(packetSize);
    recordTransmission(airtimeMs);
}

void DutyCycleMonitor::recordTransmission(unsigned long airtimeMs) {
    updateWindow();

    // Create new transmission record
    TransmissionRecord* record = new TransmissionRecord();
    record->timestamp = millis();
    record->airtimeMs = airtimeMs;
    record->next = transmissions;
    transmissions = record;

    // Update total
    totalAirtimeMs += airtimeMs;

    // Check thresholds
    checkThresholds();

    // Log status
    float percentage = getCurrentPercentage();
    LOG_DEBUG("Duty cycle: %.2f%% (%lu/%lu ms)",
              percentage, totalAirtimeMs, DUTY_CYCLE_MAX_AIRTIME_MS);
}

unsigned long DutyCycleMonitor::calculateAirtime(uint16_t packetSize) {
    return calculateAirtime(packetSize, config);
}

unsigned long DutyCycleMonitor::calculateAirtime(uint16_t packetSize, AirtimeConfig& cfg) {
    // LoRa airtime calculation based on Semtech AN1200.13
    float bandwidth = cfg.bandwidth * 1000; // Convert to Hz
    float Tsymbol = (1 << cfg.spreadingFactor) / bandwidth * 1000; // ms

    // Preamble time
    float Tpreamble = (cfg.preambleLength + 4.25) * Tsymbol;

    // Payload calculation
    float payloadSymbols;
    if (cfg.lowDataRateOptimize) {
        // Low data rate optimization for SF11/SF12
        int de = 1;
        payloadSymbols = 8 + ceil((8 * packetSize - 4 * cfg.spreadingFactor + 28 + 16 * cfg.crc - 20 * 0) /
                                  (4 * (cfg.spreadingFactor - 2 * de))) * cfg.codingRate;
    } else {
        payloadSymbols = 8 + ceil((8 * packetSize - 4 * cfg.spreadingFactor + 28 + 16 * cfg.crc) /
                                  (4 * cfg.spreadingFactor)) * cfg.codingRate;
    }

    float Tpayload = payloadSymbols * Tsymbol;

    // Total airtime
    unsigned long totalAirtime = (unsigned long)(Tpreamble + Tpayload);

    return totalAirtime;
}

float DutyCycleMonitor::getCurrentPercentage() {
    updateWindow();
    unsigned long windowElapsed = millis() - windowStartMs;
    if (windowElapsed == 0) return 0;

    return (totalAirtimeMs * 100.0) / DUTY_CYCLE_WINDOW_MS;
}

unsigned long DutyCycleMonitor::getCurrentAirtime() {
    updateWindow();
    return totalAirtimeMs;
}

unsigned long DutyCycleMonitor::getRemainingAirtime() {
    updateWindow();
    if (totalAirtimeMs >= DUTY_CYCLE_MAX_AIRTIME_MS) {
        return 0;
    }
    return DUTY_CYCLE_MAX_AIRTIME_MS - totalAirtimeMs;
}

unsigned long DutyCycleMonitor::getWindowElapsed() {
    return millis() - windowStartMs;
}

bool DutyCycleMonitor::isWarning() {
    return totalAirtimeMs >= DUTY_CYCLE_WARNING_MS;
}

bool DutyCycleMonitor::isCritical() {
    return totalAirtimeMs >= DUTY_CYCLE_CRITICAL_MS;
}

void DutyCycleMonitor::onWarning(void (*callback)(float percentage)) {
    onWarningCallback = callback;
}

void DutyCycleMonitor::onLimit(void (*callback)(float percentage)) {
    onLimitCallback = callback;
}

void DutyCycleMonitor::onReset(void (*callback)()) {
    onResetCallback = callback;
}

void DutyCycleMonitor::reset() {
    // Free all transmission records
    while (transmissions != nullptr) {
        TransmissionRecord* next = transmissions->next;
        delete transmissions;
        transmissions = next;
    }

    totalAirtimeMs = 0;
    windowStartMs = millis();
    warningIssued = false;
    criticalWarningIssued = false;

    if (onResetCallback) {
        onResetCallback();
    }

    LOG_INFO("Duty cycle window reset");
}

void DutyCycleMonitor::enableEnforcement(bool enable) {
    enforcementEnabled = enable;
    LOG_INFO("Duty cycle enforcement: %s", enable ? "ENABLED" : "DISABLED");
}

void DutyCycleMonitor::printStatus() {
    updateWindow();

    Serial.println("=== Duty Cycle Status ===");
    Serial.print("Current: ");
    Serial.print(getCurrentPercentage(), 2);
    Serial.println("%");
    Serial.print("Airtime: ");
    Serial.print(totalAirtimeMs);
    Serial.print("/");
    Serial.print(DUTY_CYCLE_MAX_AIRTIME_MS);
    Serial.println(" ms");
    Serial.print("Remaining: ");
    Serial.print(getRemainingAirtime());
    Serial.println(" ms");
    Serial.print("Window elapsed: ");
    Serial.print(getWindowElapsed() / 1000);
    Serial.println(" seconds");
    Serial.print("Status: ");
    if (isCritical()) {
        Serial.println("CRITICAL");
    } else if (isWarning()) {
        Serial.println("WARNING");
    } else {
        Serial.println("OK");
    }
    Serial.println("========================");
}

void DutyCycleMonitor::updateWindow() {
    unsigned long now = millis();

    // Check if window has expired (1 hour)
    if (now - windowStartMs >= DUTY_CYCLE_WINDOW_MS) {
        LOG_INFO("Duty cycle window expired, resetting");
        reset();
    } else {
        // Clean old records outside current window
        cleanOldRecords();
    }
}

void DutyCycleMonitor::cleanOldRecords() {
    unsigned long cutoff = millis() - DUTY_CYCLE_WINDOW_MS;
    TransmissionRecord* prev = nullptr;
    TransmissionRecord* current = transmissions;
    unsigned long removedAirtime = 0;

    while (current != nullptr) {
        if (current->timestamp < cutoff) {
            // Remove this record
            removedAirtime += current->airtimeMs;
            TransmissionRecord* toDelete = current;

            if (prev == nullptr) {
                transmissions = current->next;
                current = transmissions;
            } else {
                prev->next = current->next;
                current = current->next;
            }

            delete toDelete;
        } else {
            prev = current;
            current = current->next;
        }
    }

    if (removedAirtime > 0) {
        totalAirtimeMs -= removedAirtime;
        LOG_DEBUG("Cleaned %lu ms of old transmissions", removedAirtime);
    }
}

void DutyCycleMonitor::checkThresholds() {
    float percentage = getCurrentPercentage();

    // Check critical threshold
    if (totalAirtimeMs >= DUTY_CYCLE_CRITICAL_MS && !criticalWarningIssued) {
        LOG_WARN("CRITICAL: Duty cycle at %.2f%% - approaching limit!", percentage);
        criticalWarningIssued = true;
        if (onWarningCallback) {
            onWarningCallback(percentage);
        }
    }
    // Check warning threshold
    else if (totalAirtimeMs >= DUTY_CYCLE_WARNING_MS && !warningIssued) {
        LOG_WARN("WARNING: Duty cycle at %.2f%%", percentage);
        warningIssued = true;
        if (onWarningCallback) {
            onWarningCallback(percentage);
        }
    }
}

// Helper function implementations
void initDutyCycle(uint8_t sf, float bw, uint8_t cr) {
    AirtimeConfig config;
    config.spreadingFactor = sf;
    config.bandwidth = bw;
    config.codingRate = cr;
    config.preambleLength = 8;
    config.lowDataRateOptimize = (sf >= 11);
    config.crc = true;

    dutyCycle.begin(config, true);
}

bool checkDutyCycle(uint16_t packetSize) {
    return dutyCycle.canTransmit(packetSize);
}

void updateDutyCycle(uint16_t packetSize) {
    dutyCycle.recordTransmission(packetSize);
}

void printDutyCycleStatus() {
    dutyCycle.printStatus();
}

// Standalone airtime calculation
unsigned long calculateLoRaAirtime(uint16_t payloadSize, uint8_t sf, float bw, uint8_t cr, uint8_t preamble) {
    AirtimeConfig config;
    config.spreadingFactor = sf;
    config.bandwidth = bw;
    config.codingRate = cr;
    config.preambleLength = preamble;
    config.lowDataRateOptimize = (sf >= 11);
    config.crc = true;

    return dutyCycle.calculateAirtime(payloadSize, config);
}

float getTimeOnAir(uint16_t size, uint8_t sf, float bw) {
    unsigned long airtime = calculateLoRaAirtime(size, sf, bw, 5, 8);
    return airtime / 1000.0; // Convert to seconds
}