#pragma once
#include <cstdint>
#include "WString.h"

class IPAddress
{
public:
    IPAddress() = default;
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : b_{a, b, c, d} {}
    explicit IPAddress(uint32_t v) : b_{(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)} {}
    uint8_t operator[](int i) const { return b_[i & 3]; }
    operator uint32_t() const { return b_[0] | (b_[1] << 8) | (b_[2] << 16) | ((uint32_t)b_[3] << 24); }
    String toString() const
    {
        return String((int)b_[0]) + "." + String((int)b_[1]) + "." + String((int)b_[2]) + "." + String((int)b_[3]);
    }

private:
    uint8_t b_[4] = {0, 0, 0, 0};
};
