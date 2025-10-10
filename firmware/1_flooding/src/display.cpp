#include "display.h"

Display::Display() {
}

void Display::drawDisplay() {
    display.clearDisplay();

    // Line 1: Node ID + Role (y=0, size 1)
    printLine(displayText[0], x1, 0, 1, minX1, move1);
    
    // Line 2: TX/RX counts (y=12, size 2)
    printLine(displayText[1], x2, 12, 2, minX2, move2);
    
    // Line 3: Route info (y=32, size 2)
    printLine(displayText[2], x3, 32, 2, minX3, move3);
    
    // Line 4: Duty cycle (y=54, size 1)
    printLine(displayText[3], x4, 54, 1, minX4, move4);

    display.display();

    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void Display::printLine(String str, int& x, int y, int size, int minX, bool move) {
    display.setTextSize(size);
    display.setCursor(x, y);
    display.print(str);

    if (move) {
        x = x - 2;
        if (x < minX) x = display.width();
    }
}

void Display::changeLineOne(String text) {
    changeLine(text, 0, x1, minX1, 1, move1);
}

void Display::changeLineTwo(String text) {
    changeLine(text, 1, x2, minX2, 2, move2);
}

void Display::changeLineThree(String text) {
    changeLine(text, 2, x3, minX3, 2, move3);
}

void Display::changeLineFour(String text) {
    changeLine(text, 3, x4, minX4, 1, move4);
}

void Display::changeLine(String text, int pos, int& x, int& minX, int size, bool& move) {
    // Enable scrolling if text is too long for display
    int charWidth = 6 * size; // Approximate character width in pixels
    if (text.length() * charWidth > SCREEN_WIDTH) {
        x = display.width();
        minX = -(charWidth) * text.length();
        move = true;
    } else {
        x = 0;
        move = false;
    }

    displayText[pos] = text;
}

void Display::initDisplay() {
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }

    display.clearDisplay();
    display.setTextColor(WHITE); // Draw white text
    display.setTextWrap(false);

    Serial.println(F("SSD1306 display initialized"));
    delay(50);
}

Display Screen = Display();
