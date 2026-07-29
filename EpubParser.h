#ifndef EPUB_READER_H
#define EPUB_READER_H

#include <Arduino.h>
#include <new>
// SdFat types (SdFile, FatFile) come from InkplateLibrary's bundled SdFat,
// included transitively via SDHandler.h -> Inkplate.h below.
#include <unzipLIB.h>
#include <tinyxml2.h>
#include <ArduinoJson.h>
#include "SDHandler.h"

class EpubParser
{
public:
    EpubParser();
    bool openEpub(String filename, String book);
    bool parseEpubMetadata(String book);
    bool extractEpubContent();
    String getPageContent(String book, int page);
    // New overload: stream-extract a slice of page content starting at character index 'startChar' for 'length' characters
    String getPageContent(String book, int page, size_t startChar, size_t length);
    String getPagePath(String book, int page);
    String getCoverPath(String book);
    void closeEpub();

private:
    // SdFat &sd;
    UNZIP *zip = nullptr; // unzipLIB structure (lazily allocated in PSRAM; see ensureZip)
    bool ensureZip();
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
    // Streaming version: does not build the full string; counts characters globally and only appends the requested slice
    void extractTextFromElement(tinyxml2::XMLElement *element,
                                size_t &globalIndex,
                                size_t targetStart,
                                size_t targetEnd,
                                String &output,
                                bool &done);
    // bool isPrintable(char c);
    int parseSpineToJson(const char *xml, JsonDocument &jsonDoc);
    JsonDocument parseManifestToJson(const char *xml, String relPath);
    JsonDocument parseBookMetadataToJson(const char *xml);
};

#endif
