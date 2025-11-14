/**
 * @file duty_cycle.h
 * @brief Duty cycle monitoring and enforcement for AS923 compliance
 */

#ifndef DUTY_CYCLE_H
#define DUTY_CYCLE_H

#include <Arduino.h>

// AS923 Thailand regulatory limits
#define DUTY_CYCLE_LIMIT_PERCENT   1.0     // 1% maximum
#define DUTY_CYCLE_WINDOW_MS        3600000 // 1 hour window
#define DUTY_CYCLE_MAX_AIRTIME_MS   36000   // 36 seconds per hour
#define DUTY_CYCLE_WARNING_MS       30000   // Warn at 30 seconds (83%)
#define DUTY_CYCLE_CRITICAL_MS      34000   // Critical at 34 seconds (94%)

// Airtime calculation parameters for LoRa
struct AirtimeConfig {
    float bandwidth;        // kHz (125, 250, 500)
    uint8_t spreadingFactor;  // 7-12
    uint8_t codingRate;      // 5-8 (4/5 to 4/8)
    uint8_t preambleLength;  // symbols
    bool lowDataRateOptimize; // for SF11/SF12
    bool crc;               // CRC enabled
};

class DutyCycleMonitor {
private:
    struct TransmissionRecord {
        unsigned long timestamp;
        unsigned long airtimeMs;
        TransmissionRecord* next;
    };

    TransmissionRecord* transmissions;
    unsigned long totalAirtimeMs;
    unsigned long windowStartMs;
    AirtimeConfig config;
    bool enforcementEnabled;
    bool warningIssued;
    bool criticalWarningIssued;

    // Callbacks
    void (*onWarningCallback)(float percentage);
    void (*onLimitCallback)(float percentage);
    void (*onResetCallback)();

public:
    DutyCycleMonitor();
    ~DutyCycleMonitor();

    void begin(AirtimeConfig& config, bool enableEnforcement = true);
    void setConfig(AirtimeConfig& config);

    // Check if transmission is allowed
    bool canTransmit(uint16_t packetSize);

    // Record a transmission
    void recordTransmission(uint16_t packetSize);
    void recordTransmission(unsigned long airtimeMs);

    // Calculate airtime for a packet
    unsigned long calculateAirtime(uint16_t packetSize);
    unsigned long calculateAirtime(uint16_t packetSize, AirtimeConfig& config);

    // Get current status
    float getCurrentPercentage();
    unsigned long getCurrentAirtime();
    unsigned long getRemainingAirtime();
    unsigned long getWindowElapsed();
    bool isWarning();
    bool isCritical();

    // Callbacks
    void onWarning(void (*callback)(float percentage));
    void onLimit(void (*callback)(float percentage));
    void onReset(void (*callback)());

    // Management
    void reset();
    void enableEnforcement(bool enable);
    void printStatus();

private:
    void updateWindow();
    void cleanOldRecords();
    void checkThresholds();
};

// Global duty cycle monitor
extern DutyCycleMonitor dutyCycle;

// Helper functions
void initDutyCycle(uint8_t sf, float bw, uint8_t cr);
bool checkDutyCycle(uint16_t packetSize);
void updateDutyCycle(uint16_t packetSize);
void printDutyCycleStatus();

// Airtime calculation helpers
unsigned long calculateLoRaAirtime(uint16_t payloadSize, uint8_t sf, float bw, uint8_t cr, uint8_t preamble = 8);
float getTimeOnAir(uint16_t size, uint8_t sf, float bw);

#endif // DUTY_CYCLE_H