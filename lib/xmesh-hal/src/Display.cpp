#include "xmesh/hal/Display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Heltec WiFi LoRa 32 V3 (ESP32-S3) OLED pins
#define HELTEC_V3_SDA 17
#define HELTEC_V3_SCL 18
#define HELTEC_V3_RST 21
#define HELTEC_V3_VEXT 36  // Vext power control - LOW = ON

namespace xmesh {
namespace hal {

bool Display::begin() {
    Serial.println("[Display] Initializing OLED...");
    
    // Enable Vext power for OLED (GPIO 36 = LOW to power on)
    pinMode(HELTEC_V3_VEXT, OUTPUT);
    digitalWrite(HELTEC_V3_VEXT, LOW);
    delay(50);
    Serial.println("[Display] Vext power enabled (GPIO 36 LOW)");
    
    // Reset OLED
    pinMode(HELTEC_V3_RST, OUTPUT);
    digitalWrite(HELTEC_V3_RST, LOW);
    delay(50);
    digitalWrite(HELTEC_V3_RST, HIGH);
    delay(50);

    // Initialize I2C with Heltec V3 pins
    Wire.begin(HELTEC_V3_SDA, HELTEC_V3_SCL);
    delay(100);

    auto* impl = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    display_impl_ = static_cast<void*>(impl);

    if (!impl->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[Display] SSD1306 init failed at 0x3C");
        delete impl;
        display_impl_ = nullptr;
        return false;
    }

    Serial.println("[Display] SSD1306 initialized OK");
    impl->clearDisplay();
    impl->setTextColor(SSD1306_WHITE);
    impl->setTextSize(1);
    impl->display();

    return true;
}

Display::~Display() {
    if (display_impl_ != nullptr) {
        delete static_cast<Adafruit_SSD1306*>(display_impl_);
        display_impl_ = nullptr;
    }
}

void Display::clear() {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->clearDisplay();
}

void Display::display() {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->display();
}

void Display::setCursor(uint16_t x, uint16_t y) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->setCursor(x, y);
}

void Display::setTextSize(uint8_t size) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->setTextSize(size);
}

void Display::setTextColor(uint16_t color) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->setTextColor(color);
}

void Display::print(const char* text) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->print(text);
}

void Display::print(const String& text) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->print(text);
}

void Display::print(int value) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->print(value);
}

void Display::print(unsigned int value) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->print(value);
}

void Display::print(float value, int decimals) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->print(value, decimals);
}

void Display::println(const char* text) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->println(text);
}

void Display::println(const String& text) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->println(text);
}

void Display::println(int value) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->println(value);
}

void Display::println(unsigned int value) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->println(value);
}

void Display::println(float value, int decimals) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->println(value, decimals);
}

void Display::println() {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->println();
}

uint16_t Display::width() const {
    if (!display_impl_) return 0;
    return static_cast<Adafruit_SSD1306*>(display_impl_)->width();
}

uint16_t Display::height() const {
    if (!display_impl_) return 0;
    return static_cast<Adafruit_SSD1306*>(display_impl_)->height();
}

void Display::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->drawFastHLine(x, y, w, color);
}

void Display::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->drawFastVLine(x, y, h, color);
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!display_impl_) return;
    static_cast<Adafruit_SSD1306*>(display_impl_)->fillRect(x, y, w, h, color);
}

} // namespace hal
} // namespace xmesh
