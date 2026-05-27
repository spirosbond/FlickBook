#include "LibraryManager.h"
// #include "ArduinoJson.h"
// #include <SD.h>
extern SDHandler sdHandler;
extern EpubParser epubParser;
extern SettingsManager settingsManager;
LibraryManager::LibraryManager() : currentBook(""), currentPageString(""), currentPagePath(""), showFinishedBooks(false), totalBooks(0), finishedBooks(0) {}

void LibraryManager::init()
{
  if (!sdHandler.folderExists("/library"))
  {
    Serial.println("Library folder does not exist. Creating...");
    sdHandler.createFolder("/", "library");
  }
  std::vector<String> bookList = sdHandler.listFiles("/books", true);
  loadLibrary();
  for (const String book : bookList)
  {
    Serial.println(book);
    if (std::find_if(library.begin(), library.end(), [&book](const BookInfo &b)
                     { return b.name == book; }) == library.end())
    {
      if (!sdHandler.folderExists(book))
      {
        Serial.println("Book folder does not exist. Creating...");
        sdHandler.createFolder("/library", book);
      }
      Serial.print(book);
      Serial.println(" not in library. Adding...");
      epubParser.openEpub("/books/" + book + ".epub", book);
      epubParser.extractEpubContent();
      epubParser.parseEpubMetadata(book);
      epubParser.closeEpub();
      setCurrentBook(book);
      initBookUserData();
    }
    StaticJsonDocument<4096> manifestDoc = sdHandler.loadJson("/library/" + book + "/manifest.json");
    StaticJsonDocument<4096> spineDoc = sdHandler.loadJson("/library/" + book + "/spine.json");
    if (manifestDoc.isNull() || manifestDoc.size() == 0 || spineDoc.isNull() || spineDoc.size() == 0)
    {
      epubParser.parseEpubMetadata(book);
    }

    loadLibrary();
  }
}

void LibraryManager::loadLibrary()
{
  std::vector<String> folders = sdHandler.listFiles("/library", true);
  library.clear();
  totalBooks = 0;
  finishedBooks = 0;
  for (const String &book : folders)
  {
    StaticJsonDocument<4096> ud = sdHandler.loadJson("/library/" + book + "/user.json");
    BookInfo info = bookInfoFromJson(book, ud);
    library.push_back(info);
    totalBooks++;
    if (info.isFinished)
    {
      finishedBooks++;
    }
  }
  Serial.println("Finished loadLibrary(). Total: " + String(totalBooks) + " Finished: " + String(finishedBooks));
}

bool LibraryManager::loadBookUserData()
{
  userData = sdHandler.loadJson("/library/" + currentBook + "/user.json");
  if (!hasUserData())
  {
    Serial.print("Loaded empty User Data: ");
    return false;
  }
  Serial.print("Loaded Book User data: ");
  serializeJson(userData, Serial);
  Serial.println();
  return true;
}

BookInfo LibraryManager::bookInfoFromJson(const String &name, const StaticJsonDocument<4096> &doc)
{
  BookInfo info;
  info.name = name;
  if (!doc.isNull() && doc.size() > 0)
  {
    info.pages = doc["pages"].as<String>();
    info.lastPage = doc["lastPage"].as<int>();
    info.lastSection = doc["lastSection"].as<int>();
    info.lastSectionIndex = doc["lastSectionIndex"].as<int>();
    info.title = doc["title"].as<String>();
    info.author = doc["author"].as<String>();
    info.isFinished = bool(doc["isFinished"].as<int>());
  }
  else
  {
    info.pages = "0";
    info.lastPage = 0;
    info.lastSection = 0;
    info.lastSectionIndex = 0;
    info.title = name;
    info.author = "";
    info.isFinished = false;
  }
  return info;
}

bool LibraryManager::hasUserData()
{
  return !userData.isNull() && userData.size() > 0;
}

std::vector<BookInfo> LibraryManager::getLibrary()
{
  if (!showFinishedBooks)
  {
    std::vector<BookInfo> filteredLibrary;
    for (const BookInfo &book : library)
    {
      if (!book.isFinished)
      {
        filteredLibrary.push_back(book);
      }
    }
    return filteredLibrary;
  }

  return library;
}

bool LibraryManager::getShowFinishedBooks()
{
  return showFinishedBooks;
}

void LibraryManager::setShowFinishedBooks(bool showFinishedBooks)
{
  this->showFinishedBooks = showFinishedBooks;
}

String LibraryManager::getCurrentBook()
{
  return currentBook;
}

void LibraryManager::setCurrentBook(String book)
{
  currentBook = book;
}

bool LibraryManager::loadCurrentBook(String book)
{
  currentBook = book;
  settingsManager.setLastBook(book);
  return loadBookUserData();
}
bool LibraryManager::initBookUserData()
{
  JsonDocument metadata = sdHandler.loadJson("/library/" + currentBook + "/metadata.json");
  String keys[] = {"name", "pages", "lastPage", "lastSection", "lastSectionIndex", "title", "author", "isFinished"};
  String values[] = {currentBook, metadata["pages"], "0", "0", "0", metadata["title"], metadata["author"], "0"};
  sdHandler.saveJson("/library/" + currentBook + "/user.json", keys, values, NUMITEMS(keys));
  return loadBookUserData();
}

bool LibraryManager::saveBookUserData()
{
  // String keys[] = {"name","pages","lastPage","author"};
  // String values[] = {
  //       book["name"].as<String>(),
  //       book["pages"].as<String>(),
  //       String(page),
  //       book["author"].as<String>()
  //   };
  Serial.print("Saving Book User data: ");
  serializeJson(userData, Serial);
  Serial.println();
  updateLibraryBookInfo();
  return sdHandler.saveJson("/library/" + currentBook + "/user.json", userData);
}

void LibraryManager::updateLibraryBookInfo()
{
  for (BookInfo &book : library)
  {
    if (book.name == currentBook)
    {
      book = bookInfoFromJson(currentBook, userData);
      break;
    }
  }
}

int LibraryManager::getCurrentPage()
{
  if (hasUserData())
  {
    int currentPage = userData["lastPage"].as<int>();
    Serial.println("Getting Current page: " + String(currentPage));
    return currentPage;
  }
  else
  {
    return -1;
  }
}

int LibraryManager::getCurrentSection()
{
  if (hasUserData())
  {
    int lastSection = userData["lastSection"].as<int>();
    Serial.println("Getting last section: " + String(lastSection));
    return lastSection;
  }
  else
  {
    return -1;
  }
}

int LibraryManager::getCurrentSectionIndex()
{
  if (hasUserData())
  {
    int lastSectionIndex = userData["lastSectionIndex"].as<int>();
    Serial.println("Getting last section index: " + String(lastSectionIndex));
    return lastSectionIndex;
  }
  else
  {
    return -1;
  }
}
int LibraryManager::getTotalPage()
{
  if (hasUserData())
  {
    int totalPage = userData["pages"].as<int>();
    Serial.println("Getting Total pages: " + String(totalPage));
    return totalPage;
  }
  else
  {
    return -1;
  }
}

String LibraryManager::getTitle()
{
  if (hasUserData())
  {
    String title = userData["title"];
    Serial.println("Getting Title: " + title);
    return title;
  }
  else
  {
    return "";
  }
}

String LibraryManager::getAuthor()
{
  if (hasUserData())
  {
    String author = userData["author"];
    Serial.println("Getting Author: " + author);
    return author;
  }
  else
  {
    return "";
  }
}

String LibraryManager::getCurrentPageContent(uint start_index, uint length)
{
  // currentPageString = epubParser.getPageContent(getCurrentBook(), getCurrentPage());
  currentPageString = epubParser.getPageContent(getCurrentBook(), getCurrentPage(), start_index, length);
  // if (max_c > 0){
  //   return currentPageString.substring(0, max_c);
  // } else {
  return currentPageString;
  // }
}

String LibraryManager::getCurrentPagePath()
{
  currentPagePath = epubParser.getPagePath(getCurrentBook(), getCurrentPage());
  return currentPagePath;
}

void LibraryManager::setCurrentPage(int page)
{
  if (hasUserData())
  {
    if (page < 0 || page >= getTotalPage())
    {
      Serial.println("Invalid page index! Resetting to valid range.");
      page = 0; // Or clamp it to a valid value
    }
    userData["lastPage"] = String(page);
    saveBookUserData();
  }
}

void LibraryManager::setCurrentSection(int section)
{
  if (hasUserData())
  {
    if (section < 0)
    {
      Serial.println("Invalid section! Resetting to valid range.");
      section = 0; // Or clamp it to a valid value
    }
    userData["lastSection"] = String(section);
    saveBookUserData();
  }
}

void LibraryManager::setCurrentSectionIndex(int section_index)
{
  if (hasUserData())
  {
    if (section_index < 0)
    {
      Serial.println("Invalid section index! Resetting to valid range.");
      section_index = 0; // Or clamp it to a valid value
    }
    userData["lastSectionIndex"] = String(section_index);
    saveBookUserData();
  }
}

bool LibraryManager::nextPage()
{
  int page = getCurrentPage() + 1;
  if (page < 0 || page >= getTotalPage())
  {
    Serial.println("Invalid page index! Resetting to valid range.");
    page = 0; // Or clamp it to a valid value
    setCurrentPage(page);
    return false;
  }
  setCurrentPage(page);
  return true;
}
bool LibraryManager::prevPage()
{
  int page = getCurrentPage() - 1;
  if (page < 0)
  {
    Serial.println("Invalid page index! Resetting to valid range.");
    page = 0;
    setCurrentPage(page);
    return false;
  }
  setCurrentPage(page);
  return true;
}

bool LibraryManager::getIsFinished()
{
  if (hasUserData())
  {
    bool isFinished = bool(userData["isFinished"].as<int>());
    Serial.println("Getting isFinished: " + String(isFinished));
    return isFinished;
  }
  else
  {
    return false;
  }
}

void LibraryManager::setIsFinished(bool isFinished)
{
  if (hasUserData())
  {
    Serial.println("Setting isFinished: " + String(isFinished));
    userData["isFinished"] = String(isFinished);
    saveBookUserData();
  }
}

bool LibraryManager::loadCurrentPageSections(std::vector<size_t> &sections)
{
  sections.clear();

  String path = "/library/" + currentBook + "/pagesections.json";
  if (!sdHandler.fileExists(path))
  {
    // File missing: return false so caller can handle via if/else
    return false;
  }

  StaticJsonDocument<4096> pageSections = sdHandler.loadJson(path);
  int currentPage = getCurrentPage();

  JsonVariant v = pageSections[String(currentPage)];
  if (v.isNull())
  {
    // File exists but no entry for current page; treat as success with empty sections
    return true;
  }

  if (!v.is<JsonArray>())
  {
    // Malformed content for this page key
    return false;
  }

  JsonArray arr = v.as<JsonArray>();
  for (JsonVariant value : arr)
  {
    size_t section = static_cast<size_t>(value.as<unsigned long>());
    sections.push_back(section);
  }

  return true;
}

bool LibraryManager::saveCurrentPageSections(std::vector<size_t> sections)
{
  StaticJsonDocument<4096> pageSections = sdHandler.loadJson("/library/" + currentBook + "/pagesections.json");
  int currentPage = getCurrentPage();

  // Create or overwrite the array for the current page
  JsonArray arr = pageSections[String(currentPage)].to<JsonArray>();
  arr.clear();
  for (size_t section : sections)
  {
    arr.add(static_cast<unsigned long>(section));
  }

  serializeJson(pageSections, Serial);
  Serial.println();
  return sdHandler.saveJson("/library/" + currentBook + "/pagesections.json", pageSections);
}

bool LibraryManager::initCurrentPageSections()
{
  String path = "/library/" + currentBook + "/pagesections.json";

  // Create an empty JSON document
  StaticJsonDocument<4096> emptyDoc;
  // Optionally initialize with an empty array for the current page
  // emptyDoc[String(getCurrentPage())] = emptyDoc.createNestedArray();

  bool ok = sdHandler.saveJson(path, emptyDoc);
  if (!ok)
  {
    Serial.println("Failed to initialize pagesections.json");
    return false;
  }
  Serial.println("Initialized pagesections.json for book: " + currentBook);
  return true;
}

int LibraryManager::getTotalBook()
{
  return totalBooks;
}
int LibraryManager::getFinishedBook()
{
  return finishedBooks;
}