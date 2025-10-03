#include "SDHandler.h"

SDHandler::SDHandler(Inkplate* display) : display(display) {}

void SDHandler::init() {
    if (display->sdCardInit()) {
        Serial.println("SD Card initialized");
    } else {
        Serial.println("Failed to initialize SD Card");
    }

    // if (!sd.begin(SdSpiConfig(display->sdGetCs(), SHARED_SPI, SD_SCK_MHZ(10)))) {
        // Serial.println("SD Card failed to begin");
        // return;
    // }

    // listFiles("/books");
}

std::vector<String> SDHandler::listFiles(String path, bool no_ext, bool deep, String prefix) {
    SdFile dir;
    std::vector<String> list;
    // Serial.println("Listing files in "+path);
    // Serial.println(no_ext);
    // Serial.println(deep);
    // Serial.println(prefix);
    if (!dir.open(path.c_str(), O_RDONLY)) {
        Serial.println("Failed to open directory");
        return list;
    }

    SdFile file;
    while (file.openNext(&dir, O_RDONLY)) {
        char fileName[150];
        file.getName(fileName, sizeof(fileName));
        // list += String(fileName) + "\n";
        String fileName_str = "";
        if (prefix.isEmpty()){
          fileName_str = String(fileName);
        } else {
          fileName_str = prefix + "/" + String(fileName);
        }
        if (deep && file.isDir()){
          std::vector<String> subList = listFiles(path + "/" + fileName, no_ext, deep, fileName_str);
          list.insert(list.end(), subList.begin(), subList.end());  // Append subdirectory files
        }
        // Serial.println(fileName_str);
        if(no_ext){
          uint8_t lastdot = fileName_str.lastIndexOf(".epub"); 
          fileName_str = fileName_str.substring(0, lastdot); 
        }
        list.push_back(fileName_str);
        // Serial.println(fileName_str);
        file.close();
    }

    dir.close();
    // Serial.println(list);
    return list;
}

bool SDHandler::saveJson(String filename, String keys[], String values[], uint8_t n) {
    // Extract the directory path from the file path (remove the filename)
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash == -1) {
        Serial.println("Invalid file path: " + filename);
        return false;
    }

    String folderPath = filename.substring(0, lastSlash); // Get directory part
    // Serial.println(folderPath);
    // Ensure all parent folders exist
    if (!createFolderRecursive(folderPath)) {
        Serial.println("Failed to create necessary folders for: " + filename);
        return false;
    }
    
    SdFile jsonFile;
    if (jsonFile.open(filename.c_str(), O_WRITE | O_CREAT | O_TRUNC)) {  // Convert String to const char*
        StaticJsonDocument<800> doc;
        for (uint8_t i = 0; i < n; i++) {  // Initialize i correctly
            doc[keys[i]] = values[i];
        }
        serializeJson(doc, jsonFile);
        jsonFile.close();
        return true;
    } else {
        Serial.println("Failed to open JSON file for writing");
        return false;
    }
}

bool SDHandler::saveJson(String filename, JsonDocument &doc) {
    // Extract the directory path from the file path (remove the filename)
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash == -1) {
        Serial.println("Invalid file path: " + filename);
        return false;
    }

    String folderPath = filename.substring(0, lastSlash); // Get directory part
    // Serial.println(folderPath);
    // Ensure all parent folders exist
    if (!createFolderRecursive(folderPath)) {
        Serial.println("Failed to create necessary folders for: " + filename);
        return false;
    }
    
    SdFile jsonFile;
    
    if (jsonFile.open(filename.c_str(), O_WRITE | O_CREAT | O_TRUNC)) {  
        serializeJson(doc, jsonFile);  // Write the JSON document to the file
        jsonFile.close();
        return true;
    } else {
        Serial.println("Failed to open JSON file for writing");
        return false;
    }
}
StaticJsonDocument<800> SDHandler::loadJson(String filename) {
    StaticJsonDocument<800> doc;
    SdFile jsonFile;
    
    if (jsonFile.open(filename.c_str(), O_RDONLY)) {  
        DeserializationError error = deserializeJson(doc, jsonFile);
        jsonFile.close();

        if (error) {
            Serial.print("Failed to parse JSON: ");
            Serial.println(error.f_str());
        }
    } else {
        Serial.println("Failed to open JSON file for reading");
    }

    return doc; // Return the JSON document
}

String SDHandler::loadFile(String filename) {
    SdFile file;
    
    if (!file.open(filename.c_str(), O_RDONLY)) {
        Serial.println("Failed to open file for reading: " + filename);
        return "";
    }

    size_t fileSize = file.fileSize();
    if (fileSize == 0) {
        Serial.println("File is empty: " + filename);
        file.close();
        return "";
    }

    char* buffer = (char*)malloc(fileSize + 1); // Allocate memory (+1 for null terminator)
    if (!buffer) {
        Serial.println("Memory allocation failed.");
        file.close();
        return "";
    }

    file.read(buffer, fileSize);
    buffer[fileSize] = '\0'; // Null-terminate the string
    file.close();

    String content = String(buffer); // Convert to String
    free(buffer); // Free allocated memory

    return content;
}

bool SDHandler::saveFile(String filename, const char *data, size_t dataSize) {
    // Extract the directory path from the file path (remove the filename)
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash == -1) {
        Serial.println("Invalid file path: " + filename);
        return false;
    }

    String folderPath = filename.substring(0, lastSlash); // Get directory part
    // Serial.println(folderPath);
    // Ensure all parent folders exist
    if (!createFolderRecursive(folderPath)) {
        Serial.println("Failed to create necessary folders for: " + filename);
        return false;
    }

    // Open the file for writing
    SdFile file;
    if (!file.open(filename.c_str(), O_WRITE | O_CREAT | O_TRUNC)) {
        Serial.println("Failed to open file for writing: " + filename);
        return false;
    }

    file.write((const uint8_t*)data, dataSize);
    file.close();
    return true;
}

bool SDHandler::folderExists(const String &path) {
    SdFile dir;
    return dir.open(path.c_str(), O_READ);
}

bool SDHandler::createFolder(const String &parentPath, const String &folderName) {
    SdFile parentDir;
    FatFile newDir;
    // if (!parentDir.open(parentPath.c_str(), O_READ)) {
        // Serial.println("Parent directory does not exist: " + parentPath);

        // return false;
    // }
    // Open the specified parent directory
    if (!parentDir.open(parentPath.c_str(), O_READ )) {
        Serial.println("Failed to open parent directory: " + parentPath);
        return false;
    }

    // Create the new directory inside the parent directory
    if (!newDir.mkdir(&parentDir, folderName.c_str(), true)) { // 'true' ensures parent directories are created
        Serial.println("Failed to create directory: " + folderName + " in " + parentPath);
        return false;
    }

    Serial.println("Directory created: " + parentPath + "/" + folderName);
    return true;
}

bool SDHandler::createFolderRecursive(const String &path) {
    if (folderExists(path)) {
        return true; // Folder already exists
    }

    String subPath = "";
    int start = 1;

    while (true) {
        int slashIndex = path.indexOf('/', start);
        if (slashIndex == -1) break; // No more slashes, exit loop

        String parentPath = path.substring(0, slashIndex);
        String folderName = path.substring(slashIndex + 1, path.indexOf('/', slashIndex + 1));
        // Serial.println(parentPath);
        // Serial.println(folderName);
        if (folderName.length() == 0) break; // Stop if no valid folder name

        start = slashIndex + 1;

        if (!folderExists(parentPath)) {
            Serial.println("Parent folder does not exist: " + parentPath);
            return false; // Stop if parent folder doesn't exist
        }
        if (folderExists(parentPath+"/"+folderName)) {
            Serial.println("Folder already exists: " + parentPath+"/"+folderName+". Continuing...");
            continue;
        }

        if (!createFolder(parentPath, folderName)) {
            Serial.println("Failed to create folder: " + folderName + " in " + parentPath);
            return false;
        }
    }

    return true;
}

String SDHandler::normalizePath(String path) {
    std::vector<String> stack;
    int start = 0;

    while (start < path.length()) {
        int end = path.indexOf("/", start);
        if (end == -1) end = path.length();

        String part = path.substring(start, end);
        start = end + 1;

        if (part == "..") {
            if (!stack.empty()) stack.pop_back();  // Go up one directory
        } else if (part != "." && part != "") {
            stack.push_back(part);
        }
    }

    String normalizedPath = "/";
    for (size_t i = 0; i < stack.size(); i++) {
        normalizedPath += stack[i];
        if (i < stack.size() - 1) normalizedPath += "/";
    }

    return normalizedPath;
}

bool SDHandler::getImageDimensions(const String &path, int &width, int &height) {
    SdFile file;
    if (!file.open(path.c_str(), O_RDONLY)) {
        Serial.println("Failed to open image file: " + path);
        return false;
    }

    uint8_t header[30];
    file.read(header, sizeof(header));
    file.close();

    if (header[0] == 'B' && header[1] == 'M') {  
        // BMP format (width at offset 18, height at offset 22)
        width  = *(int*)&header[18];
        height = *(int*)&header[22];
        return true;
    } 
    else if (header[0] == 0xFF && header[1] == 0xD8) {  
        // JPG format
        return getJpegDimensions(path, width, height);
    }
    else if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G') {
        // PNG format (IHDR width at offset 16, height at offset 20)
        width  = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
        height = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
        return true;
    }

    Serial.println("Unsupported image format: " + path);
    return false;
}

bool SDHandler::getJpegDimensions(const String &path, int &width, int &height) {
    SdFile file;
    if (!file.open(path.c_str(), O_RDONLY)) {
        Serial.println("Failed to open JPEG file: " + path);
        return false;
    }

    uint8_t marker[2];

    // Read the Start of Image (SOI) marker
    if (file.read(marker, 2) != 2 || marker[0] != 0xFF || marker[1] != 0xD8) {
        Serial.println("Not a valid JPEG file: " + path);
        file.close();
        return false;
    }

    // Scan for the Start of Frame (SOF) marker
    while (file.read(marker, 2) == 2) {
        if (marker[0] != 0xFF) continue;  // Not a valid marker

        // Check if it's one of the Start of Frame markers
        if (marker[1] >= 0xC0 && marker[1] <= 0xC3) {
            file.seekCur(3); // Skip segment length and precision byte

            uint8_t sizeData[4];
            if (file.read(sizeData, 4) != 4) {
                Serial.println("Failed to read dimensions");
                file.close();
                return false;
            }

            height = (sizeData[0] << 8) | sizeData[1];
            width  = (sizeData[2] << 8) | sizeData[3];

            file.close();
            return true;
        }

        // Skip this marker's data segment
        uint8_t segmentSize[2];
        if (file.read(segmentSize, 2) != 2) break;
        int length = (segmentSize[0] << 8) | segmentSize[1];

        file.seekCur(length - 2);
    }

    file.close();
    Serial.println("Failed to find dimensions in: " + path);
    return false;
}

// bool SDHandler::ditherImage(const String &inputPath, const String &outputPath) {
//     SdFile inputFile;
    
//     // Open the image file
//     if (!inputFile.open(inputPath.c_str(), O_RDONLY)) {
//         Serial.println("Failed to open input image: " + inputPath);
//         return false;
//     }

//     // Get image dimensions
//     int width, height;
//     if (!getImageDimensions(inputPath, width, height)) {
//         Serial.println("Failed to get image dimensions.");
//         inputFile.close();
//         return false;
//     }
//     // Serial.println("Image dimensions: "+ String(width) + "x" + String(height));

//     // Allocate memory for the grayscale image
//     uint8_t *image = (uint8_t *)malloc(width * height);
//     if (!image) {
//         Serial.println("Memory allocation failed.");
//         inputFile.close();
//         return false;
//     }

//     // Read and convert to grayscale
//     for (int y = 0; y < height; y++) {
//         for (int x = 0; x < width; x++) {
//             uint8_t r, g, b;
//             getPixelColor(inputFile, x, y, width, r, g, b); // Pass width to function
//             // Serial.println("Pixel Color: ("+ String(r) + "," + String(g) + "," + String(b));
//             image[y * width + x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b); // Convert to grayscale
//             // Serial.println("Pixel Color: ("+ String(image[y * width + x]) + "," + String(image[y * width + x+1]) + "," + String(image[y * width + x+2]));
//         }
//     }

//     inputFile.close();

//     // Apply Floyd-Steinberg dithering
//     for (int y = 0; y < height; y++) {
//       for (int x = 0; x < width; x++) {
//           int oldPixel = image[y * width + x];
//           int newPixel = (oldPixel > 127) ? 255 : 0;
//           image[y * width + x] = newPixel;
//           int quantError = oldPixel - newPixel;

//           // Distribute error
//           if (x + 1 < width) image[y * width + (x + 1)] = constrain(image[y * width + (x + 1)] + quantError * 7 / 16, 0, 255);
//           if (y + 1 < height) {
//               if (x > 0) image[(y + 1) * width + (x - 1)] = constrain(image[(y + 1) * width + (x - 1)] + quantError * 3 / 16, 0, 255);
//               image[(y + 1) * width + x] = constrain(image[(y + 1) * width + x] + quantError * 5 / 16, 0, 255);
//               if (x + 1 < width) image[(y + 1) * width + (x + 1)] = constrain(image[(y + 1) * width + (x + 1)] + quantError * 1 / 16, 0, 255);
//           }
//       }
//     }

//     // Save the output image as a black-and-white BMP (for Arduino compatibility)
//     if (!savePng(outputPath, image, width, height)) {
//         Serial.println("Failed to save dithered image.");
//         free(image);
//         return false;
//     }

//     free(image);
//     Serial.println("Dithered image saved successfully: " + outputPath);
//     return true;
// }


void SDHandler::getPixelColor(SdFile &file, int x, int y, int imageWidth, uint8_t &r, uint8_t &g, uint8_t &b) {
    int pixelPosition = (y * imageWidth + x) * 3; // Each pixel is 3 bytes (R, G, B)
    file.seekSet(pixelPosition);
    
    r = file.read();
    g = file.read();
    b = file.read();
}

// bool SDHandler::savePng(const String &outputPath, uint8_t *image, int width, int height) {
//     SdFile outputFile;
    
//     if (!outputFile.open(outputPath.c_str(), O_WRITE | O_CREAT | O_TRUNC)) {
//         Serial.println("Failed to open output PNG file: " + outputPath);
//         return false;
//     }

//     // Writing a simple BMP header (PNG would require a library)
//     uint8_t bmpHeader[54] = {
//         0x42, 0x4D, // BM
//         0, 0, 0, 0, // File size
//         0, 0, 0, 0, // Reserved
//         54, 0, 0, 0, // Data offset
//         40, 0, 0, 0, // Header size
//         0, 0, 0, 0, // Width
//         0, 0, 0, 0, // Height
//         1, 0, 8, 0, // Planes + BitsPerPixel
//         0, 0, 0, 0, // Compression
//         0, 0, 0, 0, // Image size
//         0x13, 0x0B, 0, 0, // X Pixels per meter
//         0x13, 0x0B, 0, 0, // Y Pixels per meter
//         0, 0, 0, 0, // Color palette
//         0, 0, 0, 0  // Important colors
//     };

//     // Set width & height in header
//     bmpHeader[18] = width & 0xFF;
//     bmpHeader[19] = (width >> 8) & 0xFF;
//     bmpHeader[22] = height & 0xFF;
//     bmpHeader[23] = (height >> 8) & 0xFF;

//     outputFile.write(bmpHeader, 54);

//     // Write pixel data
//     for (int y = height - 1; y >= 0; y--) {
//         for (int x = 0; x < width; x++) {
//             uint8_t pixel = (image[y * width + x] > 127) ? 255 : 0;
//             // uint8_t pixel = image[y * width + x]; 
//             outputFile.write(pixel);
//         }
//     }

//     outputFile.close();
//     return true;
// }


