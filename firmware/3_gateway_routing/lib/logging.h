/**
 * @file logging.h
 * @brief Common logging utilities for serial output and data collection
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>

// Log levels
enum LogLevel {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
};

// Event types for CSV logging
enum EventType {
    EVENT_TX,       // Packet transmitted
    EVENT_RX,       // Packet received
    EVENT_FWD,      // Packet forwarded
    EVENT_ACK,      // Acknowledgment
    EVENT_DROP,     // Packet dropped
    EVENT_DUP,      // Duplicate detected
    EVENT_HELLO,    // Routing update
    EVENT_ROUTE,    // Route table updated
    EVENT_TIMEOUT,  // Route timeout
    EVENT_ERROR     // Error condition
};

// Packet event structure for logging
struct PacketEvent {
    unsigned long timestamp;
    uint16_t nodeId;
    EventType eventType;
    uint16_t srcAddress;
    uint16_t destAddress;
    float rssi;
    float snr;
    float etx;
    uint8_t hopCount;
    uint16_t packetSize;
    uint16_t sequence;
    float cost;
    uint16_t nextHop;
    uint16_t gateway;
};

class Logger {
private:
    LogLevel currentLevel;
    bool csvMode;
    bool timestampEnabled;
    uint32_t startTimeMs;
    bool headerPrinted;

public:
    Logger();

    void begin(uint32_t baudRate = 115200, bool enableCSV = true);
    void setLevel(LogLevel level);
    void enableTimestamp(bool enable);
    void enableCSV(bool enable);

    // Log methods
    void error(const char* format, ...);
    void warn(const char* format, ...);
    void info(const char* format, ...);
    void debug(const char* format, ...);

    // CSV logging for data collection
    void logPacket(PacketEvent& event);
    void printCSVHeader();

    // Utility methods
    String getTimestamp();
    String eventTypeToString(EventType type);
    void flush();

private:
    void log(LogLevel level, const char* format, va_list args);
    const char* levelToString(LogLevel level);
};

// Global logger instance
extern Logger logger;

// Convenience macros
#define LOG_ERROR(...)   logger.error(__VA_ARGS__)
#define LOG_WARN(...)    logger.warn(__VA_ARGS__)
#define LOG_INFO(...)    logger.info(__VA_ARGS__)
#define LOG_DEBUG(...)   logger.debug(__VA_ARGS__)

// CSV logging helpers
void logPacketTransmit(uint16_t dest, uint16_t size, uint16_t seq);
void logPacketReceive(uint16_t src, uint16_t dest, float rssi, float snr, uint16_t seq);
void logPacketForward(uint16_t src, uint16_t dest, uint16_t nextHop);
void logPacketDrop(uint16_t src, uint16_t dest, const char* reason);
void logRouteUpdate(uint16_t dest, uint16_t nextHop, float cost);
void logDutyCycle(float percentage, uint32_t airtimeMs);
void logSystemStatus(uint32_t freeHeap, float cpuUsage);

#endif // LOGGING_H