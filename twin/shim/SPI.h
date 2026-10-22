#pragma once
#include <cstdint>
class SPIClass
{
public:
    explicit SPIClass(uint8_t bus = 0) {}
    void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) {}
    void end() {}
};
extern SPIClass SPI;
#define HSPI 2
#define VSPI 3
