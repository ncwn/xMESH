#ifndef XMESH_HAL_DISPLAY_H
#define XMESH_HAL_DISPLAY_H

#include <Arduino.h>

namespace xmesh {
namespace hal {

/**
 * @brief Hardware abstraction for Heltec V3 OLED Display (SSD1306)
 * 
 * Provides a clean interface for rendering mesh network status on the
 * 0.96" OLED screen. All hardware-specific dependencies are isolated here.
 */
class Display {
public:
    /**
     * @brief Initialize the OLED display hardware
     * @return true if initialization succeeded
     */
    bool begin();

    /**
     * @brief Destructor to clean up display implementation
     */
    ~Display();

    /**
     * @brief Clear the display buffer
     */
    void clear();

    /**
     * @brief Show the current buffer contents on screen
     */
    void display();

    /**
     * @brief Set text cursor position
     * @param x Column position (pixels)
     * @param y Row position (pixels)
     */
    void setCursor(uint16_t x, uint16_t y);

    /**
     * @brief Set text size multiplier
     * @param size Text size (1 = normal, 2 = 2x, etc.)
     */
    void setTextSize(uint8_t size);

    /**
     * @brief Set text color
     * @param color Color value (1 = white, 0 = black for monochrome)
     */
    void setTextColor(uint16_t color);

    /**
     * @brief Print text to display buffer at current cursor position
     * @param text String to print
     */
    void print(const char* text);
    void print(const String& text);
    void print(int value);
    void print(unsigned int value);
    void print(float value, int decimals = 2);

    /**
     * @brief Print text with newline
     */
    void println(const char* text);
    void println(const String& text);
    void println(int value);
    void println(unsigned int value);
    void println(float value, int decimals = 2);
    void println();  // Just newline

    /**
     * @brief Get display width in pixels
     */
    uint16_t width() const;

    /**
     * @brief Get display height in pixels
     */
    uint16_t height() const;

    /**
     * @brief Draw a horizontal line
     */
    void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

    /**
     * @brief Draw a vertical line
     */
    void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

    /**
     * @brief Fill a rectangle
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

private:
    void* display_impl_;  // Opaque pointer to SSD1306 implementation
};

} // namespace hal
} // namespace xmesh

#endif // XMESH_HAL_DISPLAY_H
