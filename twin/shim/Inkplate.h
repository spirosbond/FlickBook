// FlickBook digital twin - Inkplate 6 FLICK board model.
//
// The drawing stack (Adafruit_GFX, Shapes, Graphics, Image incl. PNG/JPEG/BMP
// decoders) is compiled from the real InkplateLibrary sources. Only the board
// driver (EPDDriver: framebuffers, e-paper refresh, touchscreen, frontlight,
// battery, SD card) is replaced by this host model.
#pragma once

#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "WiFi.h"
#include <SdFat.h>

#include "system/InkplateBoards.h"
#include "system/defines.h"
#include "graphics/Graphics.h"
#include "graphics/GraphicsDefs.h"
#include "graphics/Image/Image.h"

#define INKPLATE_BOARD_NAME "Inkplate 6FLICK (digital twin)"
#define MULTIPLE_DISPLAY_MODES

class Inkplate;

class Touch
{
public:
    void begin(Inkplate *inkplatePtr) { _inkplate = inkplatePtr; }
    bool init(uint8_t pwrState);
    void shutdown() { _ready = false; }
    bool available();
    void setPowerState(uint8_t s) { _power = s; }
    uint8_t getPowerState() { return _power; }
    uint8_t getData(uint16_t *xPos, uint16_t *yPos, uint8_t *z = nullptr);
    bool touchInArea(int16_t x1, int16_t y1, int16_t w, int16_t h);

private:
    Inkplate *_inkplate = nullptr;
    bool _ready = false;
    uint8_t _power = 0;
};

class Frontlight
{
public:
    void begin(Inkplate *inkplatePtr) { _inkplate = inkplatePtr; }
    void setBrightness(uint8_t v);
    void setState(bool e);

private:
    Inkplate *_inkplate = nullptr;
};

class EPDDriver
{
public:
    void writePixelInternal(int16_t x, int16_t y, uint16_t color);
    int initDriver(Inkplate *inkplatePtr);

    void display(bool leaveOn = 0);
    void selectDisplayMode(uint8_t displayMode) { _displayMode = displayMode; }
    void clearDisplay();
    uint32_t partialUpdate(bool forced = false, bool leaveOn = false);
    void setFullUpdateThreshold(uint16_t numberOfPartialUpdates);
    uint8_t getDisplayMode() { return _displayMode; }

    int einkOn() { return 1; }
    void einkOff() {}

    void setSdCardOk(int16_t s) { _sdCardOk = s; }
    int16_t getSdCardOk() { return _sdCardOk; }
    int16_t sdCardInit();
    void sdCardSleep() {}
    SdFat &getSdFat();
    SPIClass *getSPIptr();
    int sdGetCs() { return 15; }

    int8_t readTemperature() { return 22; }
    double readBattery();
    bool isPowerGood() { return true; }
    void burnInClean(uint8_t, uint16_t) {}

    // Networking helpers used by Image::draw*FromWeb (not modelled).
    uint8_t *downloadFile(const char *url, int32_t *defaultLen) { return nullptr; }
    uint8_t *downloadFile(WiFiClient *s, int32_t len) { return nullptr; }
    uint8_t *downloadFileHTTPS(const char *url, int32_t *defaultLen) { return nullptr; }

    Image image;
    Touch touchscreen;
    Frontlight frontlight;

    uint8_t _beginDone = 0;
    uint8_t _displayMode = 0;

    uint8_t *DMemoryNew = nullptr;
    uint8_t *_partial = nullptr;
    uint8_t *DMemory4Bit = nullptr;
    uint16_t _partialUpdateLimiter = 10;
    uint16_t _partialUpdateCounter = 0;
    uint8_t _blockPartial = 1;
    int16_t _sdCardOk = 0;

    // ---- twin-only API -------------------------------------------------
    // Verify the guard bytes after each framebuffer; reports overruns as
    // twin warnings. Returns true if the guards were intact.
    bool twinCheckGuards();

protected:
    Inkplate *_inkplate = nullptr;
};

class Inkplate : public Graphics, public EPDDriver
{
public:
    explicit Inkplate(uint8_t mode = INKPLATE_1BIT);
    void begin();
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void setRotation(uint8_t r);
    void preloadScreen();

    // Legacy touchscreen helpers available on older Inkplate libraries.
    bool tsAvailable() { return touchscreen.available(); }
    uint8_t tsGetData(uint16_t *x, uint16_t *y) { return touchscreen.getData(x, y); }
    bool tsInit(uint8_t pwr) { return touchscreen.init(pwr); }
    bool touchInArea(int16_t x, int16_t y, int16_t w, int16_t h) { return touchscreen.touchInArea(x, y, w, h); }

    void i2cLock() {}
    void i2cUnlock() {}
    void spiLock() {}
    void spiUnlock() {}
    void displayLock() {}
    void displayUnlock() {}

private:
    uint8_t _mode;
    void writePixel(int16_t x, int16_t y, uint16_t color) override;
};
