#include "EpubParser.h"
// using namespace tinyxml2;

extern SDHandler sdHandler;

EpubParser::EpubParser() : isOpen(false) {}

SdFile EpubParser::file;
void *EpubParser::myOpen(const char *filename, int32_t *size)
{
    // Serial.println("myOpen");
    // file = new SdFile();  // Allocate a new file
    // SdFile *file = new SdFile();  // Allocate a new file
    // Serial.println(String(filename));

    // if (!file) {
    // Serial.println("Error: Failed to allocate SdFile.");
    // return nullptr;
    // }

    if (file.open(filename, O_RDONLY))
    {
        *size = file.fileSize();
        Serial.print("File opened, size: ");
        Serial.println(*size);
        return (void *)&file; // Return pointer to file
        // return (void *)file;  // Return pointer to file
    }

    // Serial.println("Error: Failed to open file.");
    // delete file;  // Cleanup on failure
    return nullptr;

    // myfile = SD.open(filename);
    // *size = myfile.size();
    // return (void *)&myfile;
}

void EpubParser::myClose(void *p)
{
    // Serial.println("myClose1");
    ZIPFILE *pzf = (ZIPFILE *)p;
    SdFile *f = (SdFile *)pzf->fHandle;
    // SdFile *f = (SdFile *)p;
    // if (f) {
    // Serial.println("myClose2");
    f->close();
    // Serial.println("myClose3");
    // delete f;  // Free allocated memory
    // Serial.println("myClose4");
    // }

    // ZIPFILE *pzf = (ZIPFILE *)p;
    // File *f = (File *)pzf->fHandle;
    // if (f) f->close();
}

int32_t EpubParser::myRead(void *p, uint8_t *buffer, int32_t length)
{
    // Serial.println("myRead1");
    ZIPFILE *pzf = (ZIPFILE *)p;
    SdFile *f = (SdFile *)pzf->fHandle;
    // SdFile *f = (SdFile *)p;
    // Serial.println("myRead2");
    // if (!f) return -1;  // Ensure file is valid
    // Serial.println("myRead3");
    return f->read(buffer, length);

    // ZIPFILE *pzf = (ZIPFILE *)p;
    // File *f = (File *)pzf->fHandle;
    // return f->read(buffer, length);
}

int32_t EpubParser::mySeek(void *p, int32_t position, int iType)
{
    // Serial.println("mySeek");
    ZIPFILE *pzf = (ZIPFILE *)p;
    SdFile *f = (SdFile *)pzf->fHandle;
    // SdFile *f = (SdFile *)p;
    // if (!f) {
    //     Serial.println("Error: Null file pointer in seek.");
    //     return -1;
    // }

    bool success = false;

    // Serial.print("Seeking to position: ");
    // Serial.println(position);
    // Serial.println(f->fileSize());
    // Serial.println(f->fileSize());
    // Serial.print("Seek type: ");
    // Serial.println(iType);

    if (iType == SEEK_SET)
    {
        if (position < 0 || position > f->fileSize())
        {
            Serial.println("Error: Invalid seek position.");
            return -1;
        }
        success = f->seekSet(position);
    }
    else if (iType == SEEK_END)
    {
        int32_t targetPos = f->fileSize() + position;
        if (targetPos < 0)
        {
            Serial.println("Error: Invalid seek position (SEEK_END).");
            return -1;
        }
        success = f->seekSet(targetPos);
    }
    else
    { // SEEK_CUR
        int32_t targetPos = f->curPosition() + position;
        if (targetPos < 0 || targetPos > f->fileSize())
        {
            Serial.println("Error: Invalid seek position (SEEK_CUR).");
            return -1;
        }
        success = f->seekCur(position);
    }

    if (!success)
    {
        Serial.println("Error: Seek failed.");
        return -1;
    }

    // Serial.print("Seek successful. New position: ");
    // Serial.println(file.curPosition());

    // return file.curPosition(); // Return new position
    return success;

    // ZIPFILE *pzf = (ZIPFILE *)p;
    // File *f = (File *)pzf->fHandle;
    // if (iType == SEEK_SET)
    //   return f->seek(position);
    // else if (iType == SEEK_END) {
    //   return f->seek(position + pzf->iSize);
    // } else { // SEEK_CUR
    //   long l = f->position();
    //   return f->seek(l + position);
    // }
}

bool EpubParser::openEpub(String filename, String book)
{
    // Serial.println("openEpub1");
    // this->filename = filename;
    this->book = book;
    char szComment[256];
    int rc = zip.openZIP(filename.c_str(), myOpen, myClose, myRead, mySeek);
    // Serial.println("openEpub2");
    // Serial.println(rc);
    if (rc != UNZ_OK)
    {
        Serial.println("Failed to open EPUB archive.");
        return false;
    }
    isOpen = true;
    Serial.println("EPUB file opened successfully.");
    // Display the global comment and all of the filenames within
    rc = zip.getGlobalComment(szComment, sizeof(szComment));
    Serial.print("Global comment: ");
    Serial.println(szComment);
    return true;
}

bool EpubParser::parseEpubMetadata(String book)
{
    // if (!isOpen) {
    //     Serial.println("EPUB not open.");
    //     return false;
    // }

    // int rc = zip.gotoFirstFile();
    // char szName[256];
    // String content = "nothing here...";
    // char *buffer;
    // unz_file_info fi;

    // while (rc == UNZ_OK) {
    //     rc = zip.getFileInfo(&fi, szName, sizeof(szName), NULL, 0, NULL, 0);
    //     if (rc == UNZ_OK) {
    //         String fileName = String(szName);
    //         // Serial.println(fileName);
    //         // if (fileName.endsWith(".xhtml") || fileName.endsWith(".html")) {
    //         if (fileName.endsWith(".opf")) {
    //             Serial.print("Extracting: ");
    //             Serial.print(fileName);
    //             Serial.print(" - ");
    //             Serial.print(fi.compressed_size, DEC);
    //             Serial.print("/");
    //             Serial.println(fi.uncompressed_size, DEC);
    //             buffer = (char *)malloc(fi.uncompressed_size + 1);
    //             if (!buffer) {
    //                 Serial.println("Memory allocation failed.");
    //                 return "Memory error.";
    //             }
    //             zip.openCurrentFile();
    //             rc = zip.readCurrentFile((uint8_t*) buffer, fi.uncompressed_size);
    //             if (rc != fi.uncompressed_size) {
    //               Serial.print("Read error, rc=");
    //               Serial.println(rc, DEC);
    //             }
    //             buffer[fi.uncompressed_size] = '\0';
    //             content = String((char *)buffer);
    //             // Serial.println(content);
    //             uint8_t idx = content.indexOf('\n');
    //             char *xml = &content[idx+1];

    //             JsonDocument spine = parseSpineToJson(xml);
    //             // serializeJson(spine, Serial);
    //             sdHandler.saveJson("/library/" + this->book + "/spine.json", spine);
    //             JsonDocument manifest = parseManifestToJson(xml);
    //             // serializeJson(manifest, Serial);
    //             sdHandler.saveJson("/library/" + this->book + "/manifest.json", manifest);
    //             free(buffer);
    //             return true;
    //         }
    //     }
    //     rc = zip.gotoNextFile();
    // }
    String content = "nothing here...";
    std::vector<String> fileList = sdHandler.listFiles("/library/" + book, false, true);
    for (const String fileName : fileList)
    {
        if (fileName.endsWith(".opf"))
        {
            Serial.print("Parsing: ");
            Serial.println(fileName);

            content = sdHandler.loadFile("/library/" + book + "/" + fileName);
            if (content.isEmpty())
            {
                Serial.println("Failed to load file or empty file: /library/" + book + "/" + fileName);
                return false;
            }
            // Serial.println(content);
            int idx = -1;
            if (content.startsWith("<?xml"))
            {
                idx = content.indexOf('\n');
            }
            // Serial.println(idx);
            String xml = content.substring(idx + 1);
            // Load Spine
            JsonDocument spine;
            int itemCount = parseSpineToJson(xml.c_str(), spine);
            // serializeJson(spine, Serial);
            sdHandler.saveJson("/library/" + book + "/spine.json", spine);
            // Load Metadata
            JsonDocument metadata = parseBookMetadataToJson(xml.c_str());
            metadata["pages"] = itemCount;
            // serializeJson(metadata, Serial);
            sdHandler.saveJson("/library/" + book + "/metadata.json", metadata);
            // Load Manifest
            int relPathIndex = fileName.lastIndexOf("/");
            String relPath = "";
            if (relPathIndex > 0)
            {
                relPath = fileName.substring(0, relPathIndex + 1);
            }
            JsonDocument manifest = parseManifestToJson(xml.c_str(), relPath);
            // serializeJson(manifest, Serial);
            sdHandler.saveJson("/library/" + book + "/manifest.json", manifest);

            return true;
        }
    }
    return false;
}

int EpubParser::parseSpineToJson(const char *xml, JsonDocument &jsonDoc)
{
    tinyxml2::XMLDocument xmlDocument;

    // Parse XML
    if (xmlDocument.Parse(xml) != tinyxml2::XML_SUCCESS)
    {
        Serial.println("Error parsing XML");
        return 0; // Return 0 items if parsing fails
    }

    tinyxml2::XMLNode *root = xmlDocument.FirstChild();
    if (!root)
    {
        Serial.println("No root element found");
        return 0;
    }

    // Locate <spine>
    tinyxml2::XMLElement *spine = root->FirstChildElement("spine");
    if (!spine)
    {
        Serial.println("No <spine> element found");
        return 0;
    }

    // Loop through <itemref> elements and add to JSON
    tinyxml2::XMLElement *itemref = spine->FirstChildElement("itemref");
    int index = 0;
    while (itemref)
    {
        const char *idref = itemref->Attribute("idref");
        if (idref)
        {
            jsonDoc[String(index)] = idref; // Add to JSON with numeric key
            index++;
        }
        itemref = itemref->NextSiblingElement("itemref");
    }

    return index; // Return the number of items added
}

JsonDocument EpubParser::parseManifestToJson(const char *xml, String relPath)
{
    tinyxml2::XMLDocument xmlDocument;
    JsonDocument jsonDoc;

    // Parse XML
    if (xmlDocument.Parse(xml) != tinyxml2::XML_SUCCESS)
    {
        Serial.println("Error parsing XML");
        return jsonDoc; // Return empty JSON
    }

    tinyxml2::XMLNode *root = xmlDocument.FirstChild();
    // Locate <manifest>
    tinyxml2::XMLElement *manifest = root->FirstChildElement("manifest");
    if (!manifest)
    {
        Serial.println("No <manifest> element found");
        return jsonDoc;
    }

    // Loop through <item> elements and add to JSON
    tinyxml2::XMLElement *item = manifest->FirstChildElement("item");
    while (item)
    {
        const char *id = item->Attribute("id");
        const char *href = item->Attribute("href");

        if (id && href)
        {
            jsonDoc[id] = relPath + href; // Add id:href pair to JSON
        }

        item = item->NextSiblingElement("item");
    }

    return jsonDoc;
}

JsonDocument EpubParser::parseBookMetadataToJson(const char *xml)
{
    tinyxml2::XMLDocument xmlDocument;
    JsonDocument jsonDoc;

    // Parse XML
    if (xmlDocument.Parse(xml) != tinyxml2::XML_SUCCESS)
    {
        Serial.println("Error parsing XML");
        return jsonDoc; // Return empty JSON
    }

    tinyxml2::XMLElement *root = xmlDocument.FirstChildElement("package");
    if (!root)
    {
        Serial.println("No <package> element found");
        return jsonDoc;
    }

    // Locate <metadata>
    tinyxml2::XMLElement *metadata = root->FirstChildElement("metadata");
    if (!metadata)
    {
        Serial.println("No <metadata> element found");
        return jsonDoc;
    }

    // Extract <dc:title>
    tinyxml2::XMLElement *titleElement = metadata->FirstChildElement("dc:title");
    if (titleElement && titleElement->GetText())
    {
        jsonDoc["title"] = titleElement->GetText();
    }

    // Extract <dc:creator>
    tinyxml2::XMLElement *creatorElement = metadata->FirstChildElement("dc:creator");
    if (creatorElement && creatorElement->GetText())
    {
        jsonDoc["author"] = creatorElement->GetText();
    }

    // Extract <meta name="cover"...> content
    tinyxml2::XMLElement *meta = metadata->FirstChildElement("meta");
    while (meta)
    {
        const char *name = meta->Attribute("name");

        if (name)
        {
            if (strcmp(name, "cover") == 0)
            {
                const char *content = meta->Attribute("content");
                jsonDoc["cover"] = content;
                break;
            }
        }
        meta = meta->NextSiblingElement("meta");
    }

    return jsonDoc;
}

bool EpubParser::extractEpubContent()
{
    if (!isOpen)
    {
        Serial.println("EPUB not open.");
        return false;
    }

    // Load manifest JSON
    // JsonDocument manifest = sdHandler.loadJson("/library/" + this->book + "/manifest.json");

    int rc = zip.gotoFirstFile();
    char szName[256];
    char *buffer;
    unz_file_info fi;
    Serial.println("Starting Extraction for epub: " + this->book);
    while (rc == UNZ_OK)
    {
        rc = zip.getFileInfo(&fi, szName, sizeof(szName), NULL, 0, NULL, 0);
        if (rc == UNZ_OK)
        {
            String fileName = String(szName);

            if (fileName.endsWith("/"))
            {
                Serial.println(fileName + " is a folder. Skipping...");
                rc = zip.gotoNextFile();
                continue;
            }

            // Check if the file exists in the manifest values
            // bool fileExistsInManifest = false;
            // for (JsonPair keyValue : manifest.as<JsonObject>()) {
            // if (fileName == keyValue.value().as<String>()) {
            // fileExistsInManifest = true;
            // break;
            // }
            // }

            // if (fileExistsInManifest) {
            // Serial.print("Extracting: ");
            // Serial.print(fileName);
            // Serial.print(" - ");
            // Serial.print(fi.compressed_size, DEC);
            // Serial.print("/");
            // Serial.println(fi.uncompressed_size, DEC);

            // Allocate memory for the extracted file content
            buffer = (char *)malloc(fi.uncompressed_size + 1);
            if (!buffer)
            {
                Serial.println("Memory allocation failed.");
                return false;
            }

            zip.openCurrentFile();
            rc = zip.readCurrentFile((uint8_t *)buffer, fi.uncompressed_size);
            zip.closeCurrentFile();

            if (rc != fi.uncompressed_size)
            {
                Serial.print("Read error, rc=");
                Serial.println(rc, DEC);
                free(buffer);
                return false;
            }

            buffer[fi.uncompressed_size] = '\0';

            // Save the extracted file to the SD card
            String savePath = "/library/" + this->book + "/" + fileName;
            if (!sdHandler.saveFile(savePath, buffer, fi.uncompressed_size))
            {
                Serial.println("Failed to save file: " + savePath);
                // free(buffer);
                // return false;
            }

            free(buffer);
            // }
        }
        rc = zip.gotoNextFile();
    }

    Serial.println("Extraction completed.");
    return true;
}

void EpubParser::closeEpub()
{
    if (isOpen)
    {
        zip.closeZIP();
        isOpen = false;
    }
}

// String EpubParser::getPageContent(String book, int page){
//   StaticJsonDocument<4096> manifestDoc = sdHandler.loadJson("/library/" + book + "/manifest.json");
//   StaticJsonDocument<4096> spineDoc = sdHandler.loadJson("/library/" + book + "/spine.json");
//   if (manifestDoc.isNull() || manifestDoc.size() == 0 || spineDoc.isNull() || spineDoc.size() == 0) {
//       Serial.println("Couldn't load manifest or spine json files");
//       return "";
//   }
//   String name = spineDoc[String(page)];
//   // Serial.println(name);
//   String xmlPath = manifestDoc[name];
//   // Serial.println(xmlPath);
//   String xml = sdHandler.loadFile("/library/" + book + "/" + xmlPath);
//   int idx = -1;
//   if (xml.startsWith("<?xml")){
//     idx = xml.indexOf('\n');
//   }
//   // Serial.println(idx);
//   xml = xml.substring(idx + 1);
//   // String content = extractTextFromXHTML(xml.c_str());
//   // Serial.println(content);
//   // return content;

//   tinyxml2::XMLDocument doc;
//   String extractedText = "";
//   // Reserve capacity to reduce fragmentation and avoid silent allocation failures
//   bool result = extractedText.reserve(xml.length());
//   Serial.println("Text reserve result: " + result);

//   // Parse the xml content
//   if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {
//       Serial.println("Error parsing HTML");
//       return "";
//   }

//   // Locate the <body> tag (ignore <head> content)
//   tinyxml2::XMLElement *body = doc.FirstChildElement("html")->FirstChildElement("body");
//   if (!body) {
//       Serial.println("No <body> found in HTML");
//       return "";
//   }

//   // Recursive function to extract text
//   extractTextFromElement(body, extractedText);
//   Serial.println(extractedText);
//   return extractedText;
// }

// New overload: stream a slice of the page text without constructing the whole book page in memory.
// It walks the DOM and only appends characters that fall within [startChar, startChar+length-1]
// Special tags like <h> and <img .../> are injected exactly as in the original logic and counted in the character range.
String EpubParser::getPageContent(String book, int page, size_t startChar, size_t length)
{
    // Serial.println("[EpubParser] getPageContent(start,length) called");
    // Serial.printf("Requested slice: start=%u length=%u\n", (unsigned)startChar, (unsigned)length);
    StaticJsonDocument<4096> manifestDoc = sdHandler.loadJson("/library/" + book + "/manifest.json");
    StaticJsonDocument<4096> spineDoc = sdHandler.loadJson("/library/" + book + "/spine.json");
    if (manifestDoc.isNull() || manifestDoc.size() == 0 || spineDoc.isNull() || spineDoc.size() == 0)
    {
        Serial.println("Couldn't load manifest or spine json files");
        return "";
    }
    String name = spineDoc[String(page)];
    String xmlPath = manifestDoc[name];
    String xml = sdHandler.loadFile("/library/" + book + "/" + xmlPath);
    if (xml.isEmpty())
    {
        Serial.println("[EpubParser] Empty xml file");
        return "";
    }
    int idx = -1;
    if (xml.startsWith("<?xml"))
    {
        idx = xml.indexOf('\n');
    }
    xml = xml.substring(idx + 1);

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    {
        Serial.println("Error parsing HTML (stream slice)");
        return "";
    }
    tinyxml2::XMLElement *htmlEl = doc.FirstChildElement("html");
    if (!htmlEl)
    {
        Serial.println("No <html> root");
        return "";
    }
    tinyxml2::XMLElement *body = htmlEl->FirstChildElement("body");
    if (!body)
    {
        Serial.println("No <body> found in HTML");
        return "";
    }

    size_t globalIndex = 0; // counts all produced characters (including tags we inject)
    size_t targetStart = startChar;
    size_t targetEnd = startChar + length; // exclusive end
    String output = "";                    // will hold only the requested slice
    // Reserve only the needed length (minimize fragmentation)
    output.reserve(length + 16); // small headroom
    bool done = false;
    extractTextFromElement(body, globalIndex, targetStart, targetEnd, output, done);
    // Serial.printf("[EpubParser] globalIndex after walk=%u producedSliceLen=%u done=%d\n", (unsigned)globalIndex, (unsigned)output.length(), done);
    return output;
}

String EpubParser::getPagePath(String book, int page)
{
    StaticJsonDocument<4096> manifestDoc = sdHandler.loadJson("/library/" + book + "/manifest.json");
    StaticJsonDocument<4096> spineDoc = sdHandler.loadJson("/library/" + book + "/spine.json");
    if (manifestDoc.isNull() || manifestDoc.size() == 0 || spineDoc.isNull() || spineDoc.size() == 0)
    {
        //   Serial.println("Couldn't load manifest or spine json files");
        return "";
    }
    String name = spineDoc[String(page)];
    // Serial.println(name);
    String pagePath = manifestDoc[name];

    int relPathIndex = pagePath.lastIndexOf("/");
    String relPath = "";
    if (relPathIndex > 0)
    {
        relPath = pagePath.substring(0, relPathIndex + 1);
    }

    return relPath;
}

String getCoverPath(String book)
{
    StaticJsonDocument<4096> manifestDoc = sdHandler.loadJson("/library/" + book + "/manifest.json");
    StaticJsonDocument<4096> metadata = sdHandler.loadJson("/library/" + book + "/metadata.json");
    String cover = metadata["cover"];
    if (cover)
    {

        return manifestDoc[cover];
    }
    else
    {
        return "";
    }
}

// String EpubParser::extractTextFromXHTML(const char *xhtmlData) {
//     String text = "";
//     bool inTag = false;

//     for (size_t i = 0; xhtmlData[i] != '\0'; i++) {
//         char c = xhtmlData[i];
//         // Serial.print(c);
//         // Serial.print(F(c));

//         if (c == '<') {
//             inTag = true;
//         } else if (c == '>') {
//             inTag = false;
//         } else if (!inTag) {
//             text += c;
//         }
//     }
//     text.trim();
//     return text;
// }

// void EpubParser::extractTextFromElement(tinyxml2::XMLElement *element, String &output) {
//     if (!element) return;

//     const char *tag = element->Value();
//     if (!tag) return;

//     String tempOutput = "";  // Temporary storage to accumulate child text

//     // Iterate over all child nodes (not just elements)
//     for (tinyxml2::XMLNode *node = element->FirstChild(); node; node = node->NextSibling()) {
//         if (node->ToText()) {  // If the node is a text node, extract it
//             String text = node->Value();
//             // Serial.println(text);
//             text.replace("“", "\"");  // Replace weird opening quotes
//             text.replace("”", "\"");  // Replace weird closing quotes
//             text.replace("’", "'");  // Replace weird single quotes
//             text.replace("‘", "'");  // Replace weird single quotes
//             text.replace("©", "(c)");  // Replace copyright sign
//             text.replace("—", "-");  // Replace long dash
//             text.replace("–", "-");  // Replace weird dash
//             text.replace("…", "...");  // Replace 3-dot char
//             text.replace("•", "*");  // Replace middle dot char
//             text.replace(" "," "); // Replace weird space with actual space
//             if (text == " " || text == " ") return;
//             tempOutput += text;
//         }
//         else if (node->ToElement()) {  // If it's an element, process it recursively
//             extractTextFromElement(node->ToElement(), tempOutput);
//         }
//     }

//     // Special handling for certain elements
//     if (strncmp(tag, "h", 1) == 0 && isdigit(tag[1])) {  // Headings <h1>, <h2>, etc.
//         output += "<h>" + tempOutput + "</h>\n    ";
//     }
//     else if (strcmp(tag, "p") == 0) {  // Paragraphs
//         output += tempOutput + "\n    ";
//     }
//     else if (strcmp(tag, "img") == 0 || strcmp(tag, "image") == 0) {  // Handle both <img> and <image>
//         const char *src = element->Attribute("xlink:href");  // EPUBs use xlink:href for images
//         if (!src) src = element->Attribute("src");  // Fallback for <img>
//         if (src) {
//             output += "<img src=\"" + String(src) + "\"/>\n";
//         }
//     }
//     else {  // Other elements like <span>, <a> contribute their text normally
//         output += tempOutput;
//     }
// }

// Non-recursive streaming extractor (slice-aware)
// Traverses descendants of 'element' (expected <body>) without recursion.
// Counts all emitted characters (including formatting markup) in globalIndex.
// Only appends characters whose position is within [targetStart, targetEnd).
// Formatting rules:
//  Headings: <h>text</h>\n
//  Paragraphs: text\n
//  Images: <img src="..."/>\n
//  Other elements: their text content directly.
void EpubParser::extractTextFromElement(tinyxml2::XMLElement *element,
                                        size_t &globalIndex,
                                        size_t targetStart,
                                        size_t targetEnd,
                                        String &output,
                                        bool &done)
{
    if (done || !element)
        return;
    // Serial.println("[stream3] In-order non-recursive extraction start");

    struct Frame
    {
        tinyxml2::XMLNode *node;
        bool entered; // false before processing children (for elements), true after
    };

    auto isHeadingTag = [](const char *tag)
    { return tag && strncmp(tag, "h", 1) == 0 && isdigit(tag[1]); };
    auto isParagraphTag = [](const char *tag)
    { return tag && strcmp(tag, "p") == 0; };
    auto isImageTag = [](const char *tag)
    { return tag && (strcmp(tag, "img") == 0 || strcmp(tag, "image") == 0); };

    auto sanitize = [](String &text)
    {
        text.replace("“", "\"");
        text.replace("”", "\"");
        text.replace("’", "'");
        text.replace("‘", "'");
        text.replace("©", "(c)");
        text.replace("—", "-");
        text.replace("–", "-");
        text.replace("…", "...");
        text.replace("•", "*");
        text.replace(" ", " ");
    };

    auto appendSlice = [&](const String &segment)
    {
        if (done || segment.isEmpty())
            return;
        for (size_t i = 0; i < segment.length() && !done; ++i)
        {
            size_t pos = globalIndex;
            char c = segment.charAt(i);
            if (pos >= targetStart && pos < targetEnd)
            {
                output += c;
            }
            globalIndex++;
            if (globalIndex >= targetEnd)
                done = true;
        }
    };

    std::vector<Frame> stack;
    stack.push_back({element, false});

    while (!stack.empty() && !done)
    {
        Frame &f = stack.back();
        tinyxml2::XMLNode *n = f.node;

        if (n->ToText())
        {
            String text = n->Value();
            sanitize(text);
            appendSlice(text);
            stack.pop_back();
            continue;
        }

        tinyxml2::XMLElement *el = n->ToElement();
        if (!el)
        {
            // Unknown node type; skip
            stack.pop_back();
            continue;
        }

        const char *tag = el->Value();
        bool isHeading = isHeadingTag(tag);
        bool isParagraph = isParagraphTag(tag);
        bool isImage = isImageTag(tag);

        if (!f.entered)
        {
            f.entered = true; // mark as entered so next time we exit
            if (isImage)
            {
                const char *src = el->Attribute("xlink:href");
                if (!src)
                    src = el->Attribute("src");
                if (src)
                {
                    String tagStr = String("<img src=\"") + src + "\"/>\n";
                    appendSlice(tagStr);
                }
                stack.pop_back();
                continue;
            }
            if (isHeading)
            {
                appendSlice("<h>");
            }
            // Push children in reverse order to process in document order
            // We only push element/text nodes
            std::vector<tinyxml2::XMLNode *> children;
            for (tinyxml2::XMLNode *c = el->FirstChild(); c; c = c->NextSibling())
            {
                children.push_back(c);
            }
            for (int i = (int)children.size() - 1; i >= 0; --i)
            {
                stack.push_back({children[i], false});
            }
        }
        else
        {
            // Exiting element
            if (isHeading)
            {
                appendSlice("</h>\n    ");
            }
            else if (isParagraph)
            {
                appendSlice("\n    ");
            }
            stack.pop_back();
        }
    }

    // Serial.printf("[stream3] Done globalIndex=%u sliceLen=%u done=%d\n", (unsigned)globalIndex, (unsigned)output.length(), done);
}
