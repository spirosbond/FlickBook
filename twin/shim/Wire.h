#pragma once
#include <cstdint>
#include <cstddef>
class TwoWire
{
public:
    bool begin(int sda = -1, int scl = -1, uint32_t freq = 0) { return true; }
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission(bool = true) { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t requestFrom(uint8_t, size_t, bool = true) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
};
extern TwoWire Wire;
