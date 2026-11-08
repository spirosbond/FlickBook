// FlickBook digital twin - host replacement for the ESP32 Arduino core header.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "WString.h"
#include "binary.h"
#include "Print.h"
#include "Stream.h"
#include "pgmspace.h"
#include "IPAddress.h"
#include "HardwareSerial.h"
#include "esp_heap.h"

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

// ESP32 core style min/max that accept mixed argument types.
template <class T, class L>
inline auto min(const T &a, const L &b) -> decltype((b < a) ? b : a)
{
    return (b < a) ? b : a;
}
template <class T, class L>
inline auto max(const T &a, const L &b) -> decltype((b < a) ? b : a)
{
    return (a < b) ? b : a;
}
using std::abs;
using std::isinf;
using std::isnan;

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(x) ((x) * (x))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bit(b) (1UL << (b))

unsigned long millis();
unsigned long micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();

long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);
long map(long x, long in_min, long in_max, long out_min, long out_max);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
uint16_t analogRead(uint8_t pin);
uint32_t analogReadMilliVolts(uint8_t pin);
void analogReadResolution(uint8_t bits);

inline bool isPrintable(int c) { return isprint(c) != 0; }
inline bool isDigit(int c) { return isdigit(c) != 0; }
inline bool isAlpha(int c) { return isalpha(c) != 0; }
inline bool isAlphaNumeric(int c) { return isalnum(c) != 0; }
inline bool isSpace(int c) { return isspace(c) != 0; }
inline bool isWhitespace(int c) { return c == ' ' || c == '\t'; }
inline bool isUpperCase(int c) { return isupper(c) != 0; }
inline bool isLowerCase(int c) { return islower(c) != 0; }

class EspClass
{
public:
    uint32_t getFreeHeap();
    uint32_t getHeapSize();
    uint32_t getMinFreeHeap();
    uint32_t getMaxAllocHeap();
    uint32_t getPsramSize();
    uint32_t getFreePsram();
    void restart();
    const char *getSdkVersion() { return "flickbook-twin"; }
    uint32_t getCpuFreqMHz() { return 240; }
};
extern EspClass ESP;
