#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H
#include "Inkplate.h"
#include "definitions.h"
#include "SDHandler.h"
#include <ArduinoJson.h>

#define BATTERY_NOM_V 4.22
#define BATTERY_MIN_V 3.15
#define BACKLIGHT_L1 5
#define BACKLIGHT_L2 10
#define BACKLIGHT_L3 25
#define BACKLIGHT_L4 40
#define BACKLIGHT_L5 55
const String settings_file = "/settings.json";

class SettingsManager {
public:
    SettingsManager(Inkplate* display);
    void init();
    void loadSettings();
    void saveSettings();
    // int getFontSize();
    int getBacklight();
    void setBacklight(int value, bool apply);
    int incBacklight();
    bool ons(uint8_t i);
    bool ons_c(uint8_t i);
    float getBatteryPerc();
    bool getGestures();
    void setGestures(bool val);
    bool getWebserver();
    void setWebserver(bool val);
private:
    Inkplate* display;
    // int fontSize;
    int backlight;
    bool gestures;
    bool webserver;
    bool o[3] = {1,1,1};
};
#endif