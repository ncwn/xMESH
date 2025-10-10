#pragma once

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Heltec V3 OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RST    21  // Reset pin for Heltec V3
#define OLED_SDA    17  // SDA pin for Heltec V3
#define OLED_SCL    18  // SCL pin for Heltec V3

class Display {
public:
    Display();
    void initDisplay();
    void changeLineOne(String str);
    void changeLineTwo(String str);
    void changeLineThree(String str);
    void changeLineFour(String str);
    void drawDisplay();
    void changeRoutingText(String text, int position);
    void changeSizeRouting(int size);
    void clearDisplay();

private:
    Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
    TaskHandle_t Display_TaskHandle = NULL;

    void changeLine(String str, int pos, int& x, int& minX, int size, bool& move);
    void printLine(String str, int& x, int y, int size, int minX, bool move);

    String displayText[4] = {"Heltec V3", "LoRa Mesher", "Initializing...", ""};

    String routingText[25];
    int routingSize = 0;

    bool move1, move2, move3, move4, move5 = true;
    int x1, minX1, x2, minX2, x3, minX3, x4, minX4, x5, minX5;
};

extern Display Screen;
