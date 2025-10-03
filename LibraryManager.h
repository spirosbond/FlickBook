#ifndef LIBRARYMANAGER_H
#define LIBRARYMANAGER_H
#include "definitions.h"
#include "SDHandler.h"
#include "EpubParser.h"
#include <ArduinoJson.h>

class LibraryManager {
public:
    LibraryManager();
    void init();
    void loadLibrary();
    bool loadBookUserData();
    StaticJsonDocument<800> fetchBookUserData(String book);
    std::vector<String> getLibrary();
    bool saveBookUserData();
    bool initBookUserData();
    String getCurrentBook();
    void setCurrentBook(String book);
    bool loadCurrentBook(String book);
    int getCurrentPage();
    void setCurrentPage(int page);
    int getCurrentSection();
    void setCurrentSection(int page);
    int getTotalPage();
    String getTitle();
    String getAuthor();
    String getCurrentPageContent(int max_c = 0);
    String getCurrentPagePath();
    bool nextPage();
    bool prevPage();
    bool getIsFinished();
    void setIsFinished(bool isFinished);
private:
    String currentBook;
    String currentPageString;
    String currentPagePath;
    StaticJsonDocument<800> userData;
    std::vector<String> library;
};
#endif