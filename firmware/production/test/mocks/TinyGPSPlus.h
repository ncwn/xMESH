#ifndef TINYGPSPLUS_H
#define TINYGPSPLUS_H
#include <cstdint>

class TinyGPSPlus {
public:
    struct Location {
        bool isValid() const { return false; }
        double lat() const { return 0.0; }
        double lng() const { return 0.0; }
        uint32_t age() const { return 0; }
    } location;
    
    struct Altitude {
        bool isValid() const { return false; }
        double meters() const { return 0.0; }
    } altitude;
    
    struct Satellites {
        bool isValid() const { return false; }
        uint32_t value() const { return 0; }
    } satellites;

    void encode(char c) {}
};

#endif
