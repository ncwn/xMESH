/**
 * @file logging.cpp
 * @brief Common logging utilities implementation
 */

#include "logging.h"
#include <stdarg.h>

// Global logger instance
Logger logger;

Logger::Logger() {
    currentLevel = LOG_INFO;
    csvMode = false;
    timestampEnabled = true;
    startTimeMs = 0;
    headerPrinted = false;
}

void Logger::begin(uint32_t baudRate, bool enableCSV) {
    Serial.begin(baudRate);
    while (!Serial && millis() < 3000) {
        // Wait for serial port to connect (USB)
    }

    startTimeMs = millis();
    csvMode = enableCSV;

    if (csvMode) {
        printCSVHeader();
    } else {
        info("Logger initialized at %lu baud", baudRate);
    }
}

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
    if (!csvMode) {
        info("Log level set to %s", levelToString(level));
    }
}

void Logger::enableTimestamp(bool enable) {
    timestampEnabled = enable;
}

void Logger::enableCSV(bool enable) {
    csvMode = enable;
    if (csvMode && !headerPrinted) {
        printCSVHeader();
    }
}

void Logger::error(const char* format, ...) {
    if (csvMode) return;  // Don't mix text logs with CSV

    va_list args;
    va_start(args, format);
    log(LOG_ERROR, format, args);
    va_end(args);
}

void Logger::warn(const char* format, ...) {
    if (csvMode) return;

    va_list args;
    va_start(args, format);
    log(LOG_WARN, format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    if (csvMode) return;

    va_list args;
    va_start(args, format);
    log(LOG_INFO, format, args);
    va_end(args);
}

void Logger::debug(const char* format, ...) {
    if (csvMode) return;

    va_list args;
    va_start(args, format);
    log(LOG_DEBUG, format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char* format, va_list args) {
    if (level > currentLevel) return;

    // Print timestamp
    if (timestampEnabled) {
        Serial.print("[");
        Serial.print(getTimestamp());
        Serial.print("] ");
    }

    // Print log level
    Serial.print("[");
    Serial.print(levelToString(level));
    Serial.print("] ");

    // Print formatted message
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    Serial.println(buffer);
}

void Logger::logPacket(PacketEvent& event) {
    if (!csvMode) {
        // Human-readable format
        Serial.print("[");
        Serial.print(getTimestamp());
        Serial.print("] ");
        Serial.print(eventTypeToString(event.eventType));
        Serial.print(" - Src:0x");
        Serial.print(event.srcAddress, HEX);
        Serial.print(" Dst:0x");
        Serial.print(event.destAddress, HEX);
        Serial.print(" RSSI:");
        Serial.print(event.rssi);
        Serial.print(" SNR:");
        Serial.print(event.snr);
        Serial.print(" Seq:");
        Serial.println(event.sequence);
    } else {
        // CSV format for data analysis
        Serial.print(millis());
        Serial.print(",");
        Serial.print(event.nodeId);
        Serial.print(",");
        Serial.print(eventTypeToString(event.eventType));
        Serial.print(",");
        Serial.print(event.srcAddress);
        Serial.print(",");
        Serial.print(event.destAddress);
        Serial.print(",");
        Serial.print(event.rssi, 1);
        Serial.print(",");
        Serial.print(event.snr, 1);
        Serial.print(",");
        Serial.print(event.etx, 2);
        Serial.print(",");
        Serial.print(event.hopCount);
        Serial.print(",");
        Serial.print(event.packetSize);
        Serial.print(",");
        Serial.print(event.sequence);
        Serial.print(",");
        Serial.print(event.cost, 2);
        Serial.print(",");
        Serial.print(event.nextHop);
        Serial.print(",");
        Serial.println(event.gateway);
    }
}

void Logger::printCSVHeader() {
    if (headerPrinted) return;

    Serial.println("timestamp,node_id,event_type,src,dest,rssi,snr,etx,hop_count,packet_size,sequence,cost,next_hop,gateway");
    headerPrinted = true;
}

String Logger::getTimestamp() {
    unsigned long ms = millis() - startTimeMs;
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu.%03lu",
             hours, minutes % 60, seconds % 60, ms % 1000);
    return String(buffer);
}

String Logger::eventTypeToString(EventType type) {
    switch (type) {
        case EVENT_TX:      return "TX";
        case EVENT_RX:      return "RX";
        case EVENT_FWD:     return "FWD";
        case EVENT_ACK:     return "ACK";
        case EVENT_DROP:    return "DROP";
        case EVENT_DUP:     return "DUP";
        case EVENT_HELLO:   return "HELLO";
        case EVENT_ROUTE:   return "ROUTE";
        case EVENT_TIMEOUT: return "TIMEOUT";
        case EVENT_ERROR:   return "ERROR";
        default:            return "UNKNOWN";
    }
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LOG_ERROR: return "ERROR";
        case LOG_WARN:  return "WARN";
        case LOG_INFO:  return "INFO";
        case LOG_DEBUG: return "DEBUG";
        default:        return "UNKNOWN";
    }
}

void Logger::flush() {
    Serial.flush();
}

// Helper function implementations
void logPacketTransmit(uint16_t dest, uint16_t size, uint16_t seq) {
    PacketEvent event;
    event.timestamp = millis();
    event.nodeId = 0;  // Should be set by firmware
    event.eventType = EVENT_TX;
    event.srcAddress = 0;  // Should be set by firmware
    event.destAddress = dest;
    event.rssi = 0;
    event.snr = 0;
    event.etx = 0;
    event.hopCount = 0;
    event.packetSize = size;
    event.sequence = seq;
    event.cost = 0;
    event.nextHop = 0;
    event.gateway = 0;

    logger.logPacket(event);
}

void logPacketReceive(uint16_t src, uint16_t dest, float rssi, float snr, uint16_t seq) {
    PacketEvent event;
    event.timestamp = millis();
    event.nodeId = 0;  // Should be set by firmware
    event.eventType = EVENT_RX;
    event.srcAddress = src;
    event.destAddress = dest;
    event.rssi = rssi;
    event.snr = snr;
    event.etx = 0;
    event.hopCount = 0;
    event.packetSize = 0;
    event.sequence = seq;
    event.cost = 0;
    event.nextHop = 0;
    event.gateway = 0;

    logger.logPacket(event);
}

void logPacketForward(uint16_t src, uint16_t dest, uint16_t nextHop) {
    PacketEvent event;
    event.timestamp = millis();
    event.nodeId = 0;  // Should be set by firmware
    event.eventType = EVENT_FWD;
    event.srcAddress = src;
    event.destAddress = dest;
    event.rssi = 0;
    event.snr = 0;
    event.etx = 0;
    event.hopCount = 0;
    event.packetSize = 0;
    event.sequence = 0;
    event.cost = 0;
    event.nextHop = nextHop;
    event.gateway = 0;

    logger.logPacket(event);
}

void logPacketDrop(uint16_t src, uint16_t dest, const char* reason) {
    if (logger.isCsvMode()) {
        PacketEvent event;
        event.timestamp = millis();
        event.nodeId = 0;  // Should be set by firmware
        event.eventType = EVENT_DROP;
        event.srcAddress = src;
        event.destAddress = dest;
        event.rssi = 0;
        event.snr = 0;
        event.etx = 0;
        event.hopCount = 0;
        event.packetSize = 0;
        event.sequence = 0;
        event.cost = 0;
        event.nextHop = 0;
        event.gateway = 0;

        logger.logPacket(event);
    } else {
        LOG_WARN("Packet dropped from 0x%04X to 0x%04X: %s", src, dest, reason);
    }
}

void logRouteUpdate(uint16_t dest, uint16_t nextHop, float cost) {
    if (logger.isCsvMode()) {
        PacketEvent event;
        event.timestamp = millis();
        event.nodeId = 0;  // Should be set by firmware
        event.eventType = EVENT_ROUTE;
        event.srcAddress = 0;
        event.destAddress = dest;
        event.rssi = 0;
        event.snr = 0;
        event.etx = 0;
        event.hopCount = 0;
        event.packetSize = 0;
        event.sequence = 0;
        event.cost = cost;
        event.nextHop = nextHop;
        event.gateway = 0;

        logger.logPacket(event);
    } else {
        LOG_INFO("Route updated: Dest=0x%04X NextHop=0x%04X Cost=%.2f", dest, nextHop, cost);
    }
}

void logDutyCycle(float percentage, uint32_t airtimeMs) {
    if (!logger.isCsvMode()) {
        LOG_INFO("Duty cycle: %.2f%% (Airtime: %lu ms)", percentage, airtimeMs);
    }
}

void logSystemStatus(uint32_t freeHeap, float cpuUsage) {
    if (!logger.isCsvMode()) {
        LOG_DEBUG("System: Heap=%lu bytes, CPU=%.1f%%", freeHeap, cpuUsage);
    }
}