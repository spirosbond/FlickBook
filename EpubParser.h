#ifndef EPUB_READER_H
#define EPUB_READER_H

#include <Arduino.h>
#include <SdFat.h>
#include <unzipLIB.h>
#include <tinyxml2.h>
#include <ArduinoJson.h>
#include "SDHandler.h"

class EpubParser {
public:
    EpubParser();
    bool openEpub(String filename,String book);
    bool parseEpubMetadata(String book);
    bool extractEpubContent();
    String getPageContent(String book, int page);
    String getPagePath(String book, int page);
    void closeEpub();

private:
    // SdFat &sd;
    UNZIP zip;  // unzipLIB structure
    SdFile epubFile;
    bool isOpen;
    // String filename;
    String book;
    static SdFile file;
    static void *myOpen(const char *filename, int32_t *size);
    static void myClose(void *p);
    static int32_t myRead(void *p, uint8_t *buffer, int32_t length);
    static int32_t mySeek(void *p, int32_t position, int iType);
    // String extractTextFromXHTML(const char *xhtmlData);
    void extractTextFromElement(tinyxml2::XMLElement *element, String &output);
    // bool isPrintable(char c);
    int parseSpineToJson(const char *xml, JsonDocument &jsonDoc);
    JsonDocument parseManifestToJson(const char *xml, String relPath);
    JsonDocument parseBookMetadataToJson(const char *xml);
};

#endif
