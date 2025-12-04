/**
 * @file display_utils.h
 * @brief Common display utilities for OLED management
 */

#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "heltec_v3_pins.h"

// Display update intervals
#define DISPLAY_UPDATE_INTERVAL_MS  1000    // Update every second
#define DISPLAY_TIMEOUT_MS          30000   // Turn off after 30 seconds of inactivity

// Display pages
enum DisplayPage {
    PAGE_STATUS = 0,
    PAGE_METRICS,
    PAGE_ROUTING,
    PAGE_DEBUG,
    PAGE_COUNT
};

// Node status structure for display
struct NodeStatus {
    uint16_t nodeId;
    uint8_t nodeRole;

    // Link metrics
    float rssi;
    float snr;
    float etx;

    // Packet statistics
    uint32_t txPackets;
    uint32_t rxPackets;
    uint32_t fwdPackets;
    uint32_t dropPackets;

    // Routing information
    uint8_t routeCount;
    uint16_t gatewayAddr;
    uint16_t nextHopAddr;
    float routeCost;

    // Duty cycle
    float dutyCyclePercent;
    uint32_t airtimeMs;

    // System
    uint32_t uptimeMs;
    uint32_t freeHeap;
    float cpuUsage;

    // Status message
    String statusMessage;
};

class DisplayManager {
private:
    Adafruit_SSD1306* display;
    DisplayPage currentPage;
    unsigned long lastUpdateMs;
    unsigned long lastActivityMs;
    bool displayEnabled;
    NodeStatus* status;

public:
    DisplayManager();
    ~DisplayManager();

    bool begin();
    void update(NodeStatus& nodeStatus);
    void nextPage();
    void showMessage(const String& message, bool temporary = false);
    void sleep();
    void wake();
    void clear();

private:
    void drawStatusPage();
    void drawMetricsPage();
    void drawRoutingPage();
    void drawDebugPage();
    void drawHeader();
    void drawProgressBar(int x, int y, int width, int height, float percentage);
    void drawSignalStrength(int x, int y, float rssi);
    String formatUptime(uint32_t ms);
    String roleToString(uint8_t role);
};

// Global display instance
extern DisplayManager displayManager;

// Helper functions
void initDisplay();
void updateDisplay(NodeStatus& status);
void displayMessage(const char* message);
void displayError(const char* error);

#endif // DISPLAY_UTILS_H