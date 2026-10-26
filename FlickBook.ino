#include "definitions.h"
#include "Inkplate.h"
#include "EpubParser.h"
#include "UIManager.h"
#include "LibraryManager.h"
#include "SDHandler.h"
#include "SettingsManager.h"
#include "WebServerManager.h"

#if GRAYSCALE == 1
Inkplate display(INKPLATE_3BIT);
#else
Inkplate display(INKPLATE_1BIT);
#endif

LibraryManager libraryManager;
SettingsManager settingsManager(&display);
UIManager uiManager(&display, GRAYSCALE);
SDHandler sdHandler(&display);
EpubParser epubParser;
WebServerManager webServerManager(&display);
// SdFat sd;

void setup()
{
    Serial.begin(115200);
    display.begin();
    display.setRotation(1); // Portrait mode
    sdHandler.init();
    settingsManager.init();
    uiManager.renderLoadingMsg("Starting...");
    libraryManager.init();
    if (settingsManager.getWebserver())
    {
        webServerManager.start();
    }
    uiManager.init();
}

void loop()
{
    uiManager.handleTouch();
    webServerManager.handleClient();
}
