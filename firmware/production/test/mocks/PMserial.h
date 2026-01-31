#ifndef PMSERIAL_H
#define PMSERIAL_H
#include <cstdint>

#define PMS7003 0

class HardwareSerial;

class SerialPM {
public:
    SerialPM(uint8_t type, HardwareSerial& serial) {}
    void init() {}
    bool read() { return false; }
    operator bool() const { return false; }
    uint16_t pm01 = 0, pm25 = 0, pm10 = 0;
};

#endif
