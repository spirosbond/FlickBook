#ifndef LIBRARYMANAGER_H
#define LIBRARYMANAGER_H
#include "definitions.h"
#include "SDHandler.h"
#include "EpubParser.h"
#include "SettingsManager.h"
#include <ArduinoJson.h>

struct BookInfo
{
    String name;
    String pages;
    int lastPage;
    int lastSection;
    int lastSectionIndex;
    String title;
    String author;
    bool isFinished;
};

class LibraryManager
{
public:
    LibraryManager();
    void init();
    void loadLibrary();
    bool loadBookUserData();
    std::vector<BookInfo> getLibrary();
    bool getShowFinishedBooks();
    void setShowFinishedBooks(bool showFinishedBooks);
    bool saveBookUserData();
    bool initBookUserData();
    String getCurrentBook();
    void setCurrentBook(String book);
    bool loadCurrentBook(String book);
    int getCurrentPage();
    void setCurrentPage(int page);
    int getCurrentSection();
    void setCurrentSection(int section);
    int getCurrentSectionIndex();
    void setCurrentSectionIndex(int section_index);
    int getTotalPage();
    String getTitle();
    String getAuthor();
    String getCurrentPageContent(uint start_index = 0, uint length = 2000);
    String getCurrentPagePath();
    bool nextPage();
    bool prevPage();
    bool getIsFinished();
    void setIsFinished(bool isFinished);
    bool loadCurrentPageSections(std::vector<size_t> &sections);
    bool saveCurrentPageSections(std::vector<size_t> sections);
    bool initCurrentPageSections();
    int getTotalBook();
    int getFinishedBook();

private:
    String currentBook;
    String currentPageString;
    String currentPagePath;
    StaticJsonDocument<4096> userData;
    int totalBooks;
    int finishedBooks;
    std::vector<BookInfo> library;
    bool showFinishedBooks;
    void updateLibraryBookInfo();
    BookInfo bookInfoFromJson(const String &name, const StaticJsonDocument<4096> &doc);
    bool hasUserData();
};
#endif