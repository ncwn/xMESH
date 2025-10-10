#pragma once

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

/**
 * @brief Display class for xMESH OLED status display
 * 
 * Display Layout (4 lines):
 * Line 1: "ID: A3F2 [S]"       - Node ID + Role (S/R/G)
 * Line 2: "TX:45 RX:38"        - Packet counts
 * Line 3: "→G1(2.1)"           - Route to gateway, cost (flooding: just shows FLOOD)
 * Line 4: "DC: 0.8%"           - Duty cycle usage
 */
class Display {
public:
    Display();
    void initDisplay();
    void changeLineOne(String str);
    void changeLineTwo(String str);
    void changeLineThree(String str);
    void changeLineFour(String str);
    void drawDisplay();

private:
    Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    void changeLine(String str, int pos, int& x, int& minX, int size, bool& move);
    void printLine(String str, int& x, int y, int size, int minX, bool move);

    String displayText[4] = {"xMESH", "TX:0 RX:0", "FLOOD", "DC:0.0%"};

    bool move1, move2, move3, move4 = false;
    int x1, minX1, x2, minX2, x3, minX3, x4, minX4;
};

extern Display Screen;
