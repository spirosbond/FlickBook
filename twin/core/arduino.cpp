// FlickBook digital twin - Arduino core runtime (time, Serial, RNG, memory).
#include <unistd.h>

#include "Arduino.h"
#include "SPI.h"
#include "WiFi.h"
#include "Wire.h"
#include "twin.h"

HardwareSerial Serial;
EspClass ESP;
SPIClass SPI;
TwoWire Wire;
WiFiClass WiFi;

size_t Print::printf(const char *format, ...)
{
    char stackBuf[256];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(stackBuf, sizeof(stackBuf), format, ap);
    va_end(ap);
    if (n < 0)
        return 0;
    if ((size_t)n < sizeof(stackBuf))
        return write(reinterpret_cast<const uint8_t *>(stackBuf), (size_t)n);
    std::string big((size_t)n + 1, '\0');
    va_start(ap, format);
    vsnprintf(&big[0], big.size(), format, ap);
    va_end(ap);
    return write(reinterpret_cast<const uint8_t *>(big.data()), (size_t)n);
}

size_t HardwareSerial::write(uint8_t c) { return write(&c, 1); }

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    twin::serialWrite(buffer, size);
    return size;
}

unsigned long millis() { return (unsigned long)(uint32_t)twin::nowMs(); }
unsigned long micros() { return (unsigned long)(uint32_t)twin::nowUs(); }
void delay(uint32_t ms) { twin::advanceMs(ms); }
void delayMicroseconds(uint32_t us) { twin::advanceUs(us); }
void yield() {}

long random(long howbig)
{
    if (howbig <= 0)
        return 0;
    return (long)(twin::randomU32() % (uint32_t)howbig);
}

long random(long howsmall, long howbig)
{
    if (howsmall >= howbig)
        return howsmall;
    return random(howbig - howsmall) + howsmall;
}

void randomSeed(unsigned long seed)
{
    if (seed != 0)
        twin::randomSeed((uint32_t)seed);
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    const long run = in_max - in_min;
    if (run == 0)
        return out_min;
    return (x - in_min) * (out_max - out_min) / run + out_min;
}

void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return LOW; }
uint16_t analogRead(uint8_t) { return 0; }
uint32_t analogReadMilliVolts(uint8_t) { return (uint32_t)(twin::peripherals().battery_v * 500.0); }
void analogReadResolution(uint8_t) {}

// ------------------------------------------------------------------ memory ---
uint32_t EspClass::getFreeHeap() { return twin::config().free_heap_bytes; }
uint32_t EspClass::getHeapSize() { return 320u * 1024u; }
uint32_t EspClass::getMinFreeHeap() { return twin::config().free_heap_bytes; }
uint32_t EspClass::getMaxAllocHeap() { return twin::config().free_heap_bytes / 2; }
uint32_t EspClass::getPsramSize() { return (uint32_t)twin::config().psram_bytes; }
uint32_t EspClass::getFreePsram() { return (uint32_t)twin::config().psram_bytes; }
void EspClass::restart()
{
    twin::serialLogMark("ESP.restart() called - exiting");
    _exit(3);
}

namespace
{
bool psramOk(size_t size, const char *fn)
{
    if (size > twin::config().psram_bytes)
    {
        twin::serialLogMark("%s(%zu) failed: larger than PSRAM (%zu bytes)", fn, size, twin::config().psram_bytes);
        return false;
    }
    return true;
}
} // namespace

void *ps_malloc(size_t size) { return psramOk(size, "ps_malloc") ? malloc(size) : nullptr; }
void *ps_calloc(size_t n, size_t size) { return psramOk(n * size, "ps_calloc") ? calloc(n, size) : nullptr; }
void *ps_realloc(void *ptr, size_t size) { return psramOk(size, "ps_realloc") ? realloc(ptr, size) : nullptr; }
void *heap_caps_malloc(size_t size, uint32_t) { return psramOk(size, "heap_caps_malloc") ? malloc(size) : nullptr; }
void *heap_caps_calloc(size_t n, size_t size, uint32_t)
{
    return psramOk(n * size, "heap_caps_calloc") ? calloc(n, size) : nullptr;
}
void heap_caps_free(void *ptr) { free(ptr); }
size_t heap_caps_get_free_size(uint32_t caps)
{
    return (caps & MALLOC_CAP_SPIRAM) ? twin::config().psram_bytes : twin::config().free_heap_bytes;
}
size_t heap_caps_get_largest_free_block(uint32_t caps) { return heap_caps_get_free_size(caps); }

// -------------------------------------------------------------------- WiFi ---
bool WiFiClass::softAP(const char *ssid, const char *, int, int, int)
{
    mode_ = WIFI_AP;
    twin::peripherals().wifi_ap = true;
    twin::peripherals().wifi_ssid = ssid ? ssid : "";
    twin::serialLogMark("WiFi soft-AP '%s' simulated; firmware web server is reachable at http://%s:%d/",
                        ssid ? ssid : "", twin::config().web_bind.c_str(), twin::config().web_port);
    return true;
}

bool WiFiClass::softAPdisconnect(bool)
{
    mode_ = WIFI_OFF;
    twin::peripherals().wifi_ap = false;
    return true;
}
