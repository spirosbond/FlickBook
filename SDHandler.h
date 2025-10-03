#ifndef SDHANDLER_H
#define SDHANDLER_H
#include "Inkplate.h"
#include "SdFat.h"
#include <vector>
#include <ArduinoJson.h>
#include <JPEGDEC.h>

class SDHandler {
public:
    SDHandler(Inkplate* display);
    void init();
    std::vector<String> listFiles(String path, bool no_ext = false,  bool deep = false, String prefix = "");
    bool saveJson(String filename, String keys[], String values[], uint8_t n);
    bool saveJson(String filename, JsonDocument &doc);
    StaticJsonDocument<800> loadJson(String filename);
    String loadFile(String filename);
    bool folderExists(const String &path);
    bool createFolder(const String &parentPath, const String &folderName);
    bool saveFile(String filePath, const char *data, size_t dataSize);
    bool createFolderRecursive(const String &path);
    String normalizePath(String path);
    bool getImageDimensions(const String &path, int &width, int &height);
    // bool ditherImage(const String &inputPath, const String &outputPath);
    void getPixelColor(SdFile &file, int x, int y, int imageWidth, uint8_t &r, uint8_t &g, uint8_t &b);
    // bool savePng(const String &outputPath, uint8_t *image, int width, int height);
private:
    Inkplate* display;
    bool getJpegDimensions(const String &path, int &width, int &height);

};
#endif
