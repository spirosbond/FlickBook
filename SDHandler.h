#ifndef SDHANDLER_H
#define SDHANDLER_H
#include "Inkplate.h"
// SdFat is bundled with InkplateLibrary (11.x) and pulled in via Inkplate.h.
// Do not include an external SdFat here to avoid duplicate-symbol conflicts.
#include <vector>
#include <ArduinoJson.h>
// #include <JPEGDEC.h>

struct FileEntry
{
    String name;
    bool isDir;
    uint32_t size;
};

class SDHandler
{
public:
    SDHandler(Inkplate *display);
    void init();
    std::vector<String> listFiles(String path, bool no_ext = false, bool deep = false, String prefix = "");
    std::vector<FileEntry> listFilesWithMeta(const String &path);
    bool saveJson(String filename, String keys[], String values[], uint8_t n);
    bool saveJson(String filename, JsonDocument &doc);
    StaticJsonDocument<4096> loadJson(String filename);
    String loadFile(String filename);
    bool folderExists(const String &path);
    bool createFolder(const String &parentPath, const String &folderName);
    bool saveFile(String filePath, const char *data, size_t dataSize);
    bool createFolderRecursive(const String &path);
    String normalizePath(String path);
    bool getImageDimensions(const String &path, int &width, int &height);
    bool fileExists(const String &path);
    bool deletePath(const String &path);
    bool renamePath(const String &oldPath, const String &newPath);
    bool openFileForRead(const String &path, SdFile &file);
    uint32_t getFileSize(const String &path);
    // bool ditherImage(const String &inputPath, const String &outputPath);
    void getPixelColor(SdFile &file, int x, int y, int imageWidth, uint8_t &r, uint8_t &g, uint8_t &b);
    // bool savePng(const String &outputPath, uint8_t *image, int width, int height);
private:
    Inkplate *display;
    bool getJpegDimensions(const String &path, int &width, int &height);
};
#endif
