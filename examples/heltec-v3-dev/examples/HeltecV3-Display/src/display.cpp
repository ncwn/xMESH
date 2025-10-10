#include "display.h"

Display Screen;

/**
 * @brief Constructor for the Display class
 */
Display::Display() {
    x1 = 0;
    minX1 = 0;
    x2 = 0;
    minX2 = 0;
    x3 = 0;
    minX3 = 0;
    x4 = 0;
    minX4 = 0;
    x5 = 0;
    minX5 = 0;
}

/**
 * @brief Initialize the OLED display
 */
void Display::initDisplay() {
    // Initialize I2C with custom pins for Heltec V3
    Wire.begin(OLED_SDA, OLED_SCL);
    
    // Initialize display with I2C address 0x3C
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Heltec V3");
    display.setCursor(0, 16);
    display.println("Starting...");
    display.display();
    
    Serial.println("Display initialized");
}

/**
 * @brief Change the text of line one
 * 
 * @param str Text to display
 */
void Display::changeLineOne(String str) {
    changeLine(str, 0, x1, minX1, 1, move1);
}

/**
 * @brief Change the text of line two
 * 
 * @param str Text to display
 */
void Display::changeLineTwo(String str) {
    changeLine(str, 1, x2, minX2, 1, move2);
}

/**
 * @brief Change the text of line three
 * 
 * @param str Text to display
 */
void Display::changeLineThree(String str) {
    changeLine(str, 2, x3, minX3, 1, move3);
}

/**
 * @brief Change the text of line four
 * 
 * @param str Text to display
 */
void Display::changeLineFour(String str) {
    changeLine(str, 3, x4, minX4, 1, move4);
}

/**
 * @brief Clear the display
 */
void Display::clearDisplay() {
    display.clearDisplay();
    display.display();
}

/**
 * @brief Internal method to change a line
 * 
 * @param str Text to display
 * @param pos Line position
 * @param x Current x position
 * @param minX Minimum x position
 * @param size Text size
 * @param move Whether to scroll
 */
void Display::changeLine(String str, int pos, int& x, int& minX, int size, bool& move) {
    displayText[pos] = str;
    x = 0;
    int16_t x1Aux, y1Aux;
    uint16_t w, h;
    display.setTextSize(size);
    display.getTextBounds(str, 0, 0, &x1Aux, &y1Aux, &w, &h);
    minX = -w;

    if (minX < 0)
        move = true;
    else
        move = false;
}

/**
 * @brief Print a line on the display
 * 
 * @param str Text to display
 * @param x Current x position
 * @param y Y position
 * @param size Text size
 * @param minX Minimum x position
 * @param move Whether to scroll
 */
void Display::printLine(String str, int& x, int y, int size, int minX, bool move) {
    display.setTextSize(size);
    display.setCursor(x, y);
    display.println(str);

    if (move) {
        x = x - 1;
        if (x < minX) x = display.width();
    }
}

/**
 * @brief Draw the display with all lines
 */
void Display::drawDisplay() {
    display.clearDisplay();
    
    // Line 1: Address (y=0)
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(displayText[0]);
    
    // Line 2: TX Status (y=16)
    display.setCursor(0, 16);
    display.println(displayText[1]);
    
    // Line 3: RX Status or Nodes (y=32)
    display.setCursor(0, 32);
    if (displayText[2].length() > 0) {
        display.println(displayText[2]);
    }
    
    // Line 4: Routes or RX Counter (y=48)
    display.setCursor(0, 48);
    if (routingSize > 0) {
        display.print("Nodes: ");
        display.println(routingSize);
    } else if (displayText[3].length() > 0) {
        display.println(displayText[3]);
    }

    display.display();
}

/**
 * @brief Change routing text
 * 
 * @param text Text to display
 * @param position Position in array
 */
void Display::changeRoutingText(String text, int position) {
    routingText[position] = text;
}

/**
 * @brief Change the size of the routing table
 * 
 * @param size New size
 */
void Display::changeSizeRouting(int size) {
    routingSize = size;
}
