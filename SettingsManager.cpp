#include "SettingsManager.h"
extern SDHandler sdHandler;

SettingsManager::SettingsManager(Inkplate* display) : display(display), /*fontSize(4),*/ backlight(0), gestures(true), webserver(false) {}

void SettingsManager::init() {
    if (display->sdCardInit()){
      Serial.println("SD Card initialized");
    } else {
      Serial.println("Failed to initialize SD Card");
    }
    loadSettings();
    setBacklight(backlight,true);
}

void SettingsManager::loadSettings() {
    // SdFile settingsFile;
    Serial.println("Loading Settings");
    // if (settingsFile.open(settings_file, O_RDONLY)) {
    StaticJsonDocument<800> doc;
    doc = sdHandler.loadJson(settings_file);
    if (doc.isNull() || doc.size() == 0) {
      Serial.println("No Settings found. Creating with defaults...");
      saveSettings();
    } else {
      //     deserializeJson(doc, settingsFile);
      // fontSize = doc["fontSize"];
      gestures = bool(doc["gestures"].as<int>());
      backlight = doc["backlight"].as<int>();
      webserver = bool(doc["webserver"].as<int>());
      Serial.printf("Loaded gestures:%d, backlight:%d, webserver:%d\n",gestures,backlight,webserver);
      //     settingsFile.close();  
    }

}

void SettingsManager::saveSettings() {
 
    String keys[] = {"gestures","backlight","webserver"};
    String values[] = {String(gestures), String(backlight), String(webserver)};

    sdHandler.saveJson(settings_file, keys, values, NUMITEMS(keys));
}

// int SettingsManager::getFontSize() {
//     return fontSize;
// }

bool SettingsManager::getGestures() {
    return gestures;
}

void SettingsManager::setGestures(bool val) {
    gestures = val;
    saveSettings();
}

bool SettingsManager::getWebserver() {
    return webserver;
}

void SettingsManager::setWebserver(bool val) {
    webserver = val;
    saveSettings();
}

int SettingsManager::getBacklight() {
    return backlight;
}

void SettingsManager::setBacklight(int value, bool apply){
  
  if (backlight > 0 and backlight < 64){
    backlight = value;
    if(apply) {
      if (ons(0)){
        display->frontlight(true);
        delay(20);
      }
      display->setFrontlight(backlight);
    }
    
  } else {
    backlight = 0;
    if(apply) {
      display->setFrontlight(backlight);
    }
  }
  saveSettings();
}

int SettingsManager::incBacklight(){
  // Serial.println("Incrementing Backlight");
  if (backlight < BACKLIGHT_L1){
    backlight = BACKLIGHT_L1;
  } else if (backlight < BACKLIGHT_L2){
    backlight = BACKLIGHT_L2;
  } else if (backlight < BACKLIGHT_L3) {
    backlight = BACKLIGHT_L3;
  } else if (backlight < BACKLIGHT_L4) {
    backlight = BACKLIGHT_L4;
  }  else if (backlight < BACKLIGHT_L5) {
    backlight = BACKLIGHT_L5;
  }  else {
    backlight = 0;
  }
  Serial.printf("Setting Backlight to: %d\n",backlight);
  setBacklight(backlight, true);
  return backlight;
}

float SettingsManager::getBatteryPerc(){
  float voltage = display->readBattery();
  return max(min(100 * (voltage - BATTERY_MIN_V) / (BATTERY_NOM_V - BATTERY_MIN_V), 100.0), 0.0);
  // return (100 * (voltage - BATTERY_MIN_V) / (BATTERY_NOM_V - BATTERY_MIN_V));
}

bool SettingsManager::ons(uint8_t i){
  bool v = o[i];
  o[i] = 0;
  return v;
}

bool SettingsManager::ons_c(uint8_t i){
  o[i] = 1;
  return o[i];
}