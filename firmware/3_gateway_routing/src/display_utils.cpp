/**
 * @file display_utils.cpp
 * @brief Common display utilities implementation
 */

#include "display_utils.h"

// Global display instance
DisplayManager displayManager;

DisplayManager::DisplayManager() {
    display = nullptr;
    currentPage = PAGE_STATUS;
    lastUpdateMs = 0;
    lastActivityMs = 0;
    displayEnabled = true;
    status = nullptr;
}

DisplayManager::~DisplayManager() {
    if (display != nullptr) {
        delete display;
    }
}

bool DisplayManager::begin() {
    // Initialize I2C for OLED
    // Check if Wire is already initialized to avoid "Bus already started" warning
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    // Create display object
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST_PIN);

    // Initialize display
    if (!display->begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("[DISPLAY] SSD1306 allocation failed"));
        return false;
    }

    // Clear display buffer
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);

    // Show boot screen
    display->println(F("xMESH LoRaMesher"));
    display->println(F("----------------"));
    display->println(BOARD_NAME);
    display->println(F("Initializing..."));
    display->display();

    lastActivityMs = millis();
    return true;
}

void DisplayManager::update(NodeStatus& nodeStatus) {
    status = &nodeStatus;
    lastActivityMs = millis();

    // Check if display should sleep
    if (displayEnabled && (millis() - lastActivityMs > DISPLAY_TIMEOUT_MS)) {
        sleep();
        return;
    }

    // Wake display if sleeping
    if (!displayEnabled) {
        wake();
    }

    // Update at specified interval
    if (millis() - lastUpdateMs < DISPLAY_UPDATE_INTERVAL_MS) {
        return;
    }
    lastUpdateMs = millis();

    display->clearDisplay();

    // Draw current page
    switch (currentPage) {
        case PAGE_STATUS:
            drawStatusPage();
            break;
        case PAGE_METRICS:
            drawMetricsPage();
            break;
        case PAGE_ROUTING:
            drawRoutingPage();
            break;
        case PAGE_DEBUG:
            drawDebugPage();
            break;
        default:
            drawStatusPage();
    }

    display->display();
}

void DisplayManager::drawStatusPage() {
    if (status == nullptr) return;

    drawHeader();

    // Line 2: Role and uptime
    display->setCursor(0, 10);
    display->print(F("Role: "));
    display->print(roleToString(status->nodeRole));
    display->print(F(" "));
    display->print(formatUptime(status->uptimeMs));

    // Line 3: Link quality
    display->setCursor(0, 20);
    display->print(F("RSSI:"));
    display->print(status->rssi, 0);
    display->print(F(" SNR:"));
    display->print(status->snr, 1);
    drawSignalStrength(90, 20, status->rssi);

    // Line 4: Packet statistics
    display->setCursor(0, 30);
    display->print(F("TX:"));
    display->print(status->txPackets);
    display->print(F(" RX:"));
    display->print(status->rxPackets);
    display->print(F(" FWD:"));
    display->print(status->fwdPackets);

    // Line 5: Duty cycle with progress bar
    display->setCursor(0, 40);
    display->print(F("Duty: "));
    display->print(status->dutyCyclePercent, 1);
    display->print(F("%"));
    drawProgressBar(50, 40, 75, 6, status->dutyCyclePercent);

    // Line 6: Status message
    display->setCursor(0, 50);
    display->print(status->statusMessage.substring(0, 21));
}

void DisplayManager::drawMetricsPage() {
    if (status == nullptr) return;

    drawHeader();

    // Detailed metrics
    display->setCursor(0, 10);
    display->print(F("ETX: "));
    display->print(status->etx, 2);
    display->print(F(" Cost: "));
    display->print(status->routeCost, 2);

    display->setCursor(0, 20);
    display->print(F("RSSI: "));
    display->print(status->rssi, 1);
    display->print(F(" dBm"));

    display->setCursor(0, 30);
    display->print(F("SNR: "));
    display->print(status->snr, 1);
    display->print(F(" dB"));

    display->setCursor(0, 40);
    display->print(F("Drop: "));
    display->print(status->dropPackets);
    display->print(F(" ("));
    if (status->rxPackets > 0) {
        float dropRate = (status->dropPackets * 100.0) / status->rxPackets;
        display->print(dropRate, 1);
        display->print(F("%"));
    } else {
        display->print(F("0.0%"));
    }
    display->print(F(")"));

    display->setCursor(0, 50);
    display->print(F("Air: "));
    display->print(status->airtimeMs);
    display->print(F("ms/h"));
}

void DisplayManager::drawRoutingPage() {
    if (status == nullptr) return;

    drawHeader();

    // Routing information
    display->setCursor(0, 10);
    display->print(F("Routes: "));
    display->print(status->routeCount);

    display->setCursor(0, 20);
    display->print(F("GW: 0x"));
    if (status->gatewayAddr > 0) {
        display->print(status->gatewayAddr, HEX);  // Shows actual gateway MAC (0x6674, 0x8154, etc.)
    } else {
        display->print(F("----"));  // No gateway found
    }

    display->setCursor(0, 30);
    display->print(F("Via: 0x"));
    if (status->nextHopAddr > 0) {
        display->print(status->nextHopAddr, HEX);  // Shows next hop MAC address
    } else {
        display->print(F("----"));  // No route
    }

    display->setCursor(0, 40);
    display->print(F("Cost: "));
    display->print(status->routeCost, 2);

    display->setCursor(0, 50);
    display->print(F("Hops: "));
    display->print((int)(status->routeCost)); // Assuming integer part is hop count
}

void DisplayManager::drawDebugPage() {
    if (status == nullptr) return;

    drawHeader();

    // System debug info
    display->setCursor(0, 10);
    display->print(F("Heap: "));
    display->print(status->freeHeap / 1024);
    display->print(F(" KB"));

    display->setCursor(0, 20);
    display->print(F("CPU: "));
    display->print(status->cpuUsage, 1);
    display->print(F("%"));

    display->setCursor(0, 30);
    display->print(F("Addr: 0x"));
    display->print(status->nodeId, HEX);  // Shows MAC address (0x02B4, 0x6674, etc.)

    display->setCursor(0, 40);
    display->print(F("FW: v1.0.0"));  // TODO: Get from version.h

    display->setCursor(0, 50);
    display->print(F("Protocol: "));
    #ifdef PROTOCOL_FLOODING
        display->print(F("Flooding"));
    #elif PROTOCOL_HOPCOUNT
        display->print(F("Hop-Count"));
    #elif PROTOCOL_GATEWAY
        display->print(F("Gateway"));
    #else
        display->print(F("Unknown"));
    #endif
}

void DisplayManager::drawHeader() {
    // Top line: Node ID (1-6) and page indicator
    // Note: Use NODE_ID for display header (user-friendly)
    display->setCursor(0, 0);
    display->print(F("Node "));
    #ifdef NODE_ID
        display->print(NODE_ID);  // Shows 1, 2, 3, etc. (user-friendly)
    #else
        display->print(F("?"));
    #endif
    display->print(F("  ["));
    display->print(currentPage + 1);
    display->print(F("/"));
    display->print(PAGE_COUNT);
    display->print(F("]"));

    // Draw separator line
    display->drawLine(0, 8, 127, 8, SSD1306_WHITE);
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, float percentage) {
    // Draw border
    display->drawRect(x, y, width, height, SSD1306_WHITE);

    // Calculate fill width
    int fillWidth = (width - 2) * (percentage / 100.0);
    if (fillWidth > 0) {
        display->fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawSignalStrength(int x, int y, float rssi) {
    // Signal strength bars (5 bars total)
    int bars = 0;
    if (rssi > -60) bars = 5;
    else if (rssi > -70) bars = 4;
    else if (rssi > -80) bars = 3;
    else if (rssi > -90) bars = 2;
    else if (rssi > -100) bars = 1;

    for (int i = 0; i < 5; i++) {
        int barHeight = 2 + i * 2;
        int barX = x + i * 4;
        int barY = y + (8 - barHeight);

        if (i < bars) {
            display->fillRect(barX, barY, 2, barHeight, SSD1306_WHITE);
        } else {
            display->drawRect(barX, barY, 2, barHeight, SSD1306_WHITE);
        }
    }
}

void DisplayManager::nextPage() {
    currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
    lastActivityMs = millis();
    lastUpdateMs = 0; // Force immediate update
}

void DisplayManager::showMessage(const String& message, bool temporary) {
    display->clearDisplay();
    display->setCursor(0, 0);
    display->print(message);
    display->display();

    if (temporary) {
        delay(2000);
        lastUpdateMs = 0; // Force redraw
    }
}

void DisplayManager::sleep() {
    if (!displayEnabled) return;

    display->ssd1306_command(SSD1306_DISPLAYOFF);
    displayEnabled = false;
}

void DisplayManager::wake() {
    if (displayEnabled) return;

    display->ssd1306_command(SSD1306_DISPLAYON);
    displayEnabled = true;
    lastActivityMs = millis();
}

void DisplayManager::clear() {
    display->clearDisplay();
    display->display();
}

String DisplayManager::formatUptime(uint32_t ms) {
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;

    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu",
             hours, minutes % 60, seconds % 60);
    return String(buffer);
}

String DisplayManager::roleToString(uint8_t role) {
    switch (role) {
        case 0: return F("SENSOR");
        case 1: return F("RELAY");
        case 2: return F("GATEWAY");
        default: return F("UNKNOWN");
    }
}

// Helper function implementations
void initDisplay() {
    if (!displayManager.begin()) {
        Serial.println(F("[ERROR] Display initialization failed"));
    }
}

void updateDisplay(NodeStatus& status) {
    displayManager.update(status);
}

void displayMessage(const char* message) {
    displayManager.showMessage(String(message), false);
}

void displayError(const char* error) {
    String errorMsg = F("ERROR: ");
    errorMsg += error;
    displayManager.showMessage(errorMsg, true);
}