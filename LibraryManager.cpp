#include "LibraryManager.h"
// #include "ArduinoJson.h"
// #include <SD.h>
extern SDHandler sdHandler;
extern EpubParser epubParser;
LibraryManager::LibraryManager() : currentBook(""), currentPageString(""), currentPagePath("") {}

void LibraryManager::init() {
  if (!sdHandler.folderExists("/library")) {
        Serial.println("Library folder does not exist. Creating...");
        sdHandler.createFolder("/", "library");
  }
  std::vector<String> bookList = sdHandler.listFiles("/books", true);
  loadLibrary();
  for(const String book : bookList) {
    Serial.println(book);
    if(std::find(library.begin(), library.end(), book) == library.end()) {
        if (!sdHandler.folderExists(book)) {
          Serial.println("Book folder does not exist. Creating...");
          sdHandler.createFolder("/library",book);
        }
        Serial.print(book);
        Serial.println(" not in library. Adding...");
        epubParser.openEpub("/books/"+book+".epub", book);
        epubParser.extractEpubContent();
        epubParser.parseEpubMetadata(book);
        epubParser.closeEpub();
        setCurrentBook(book);
        initBookUserData();
    }
    StaticJsonDocument<800> manifestDoc = sdHandler.loadJson("/library/" + book + "/manifest.json");
    StaticJsonDocument<800> spineDoc = sdHandler.loadJson("/library/" + book + "/spine.json");
    if (manifestDoc.isNull() || manifestDoc.size() == 0 || spineDoc.isNull() || spineDoc.size() == 0) {
        epubParser.parseEpubMetadata(book);
    }

    loadLibrary();
    
  }
    
}

void LibraryManager::loadLibrary() {
  library = sdHandler.listFiles("/library", true);
  // for(const String book : library) {
  //   Serial.println(book);
  // }
}

bool LibraryManager::loadBookUserData() {
  userData = sdHandler.loadJson("/library/" + currentBook + "/user.json");
  if (userData.isNull() || userData.size() == 0){
    Serial.print("Loaded empty User Data: ");
    return false;
  }
  Serial.print("Loaded Book User data: ");
  serializeJson(userData, Serial);
  Serial.println();
  return true;
}

StaticJsonDocument<800> LibraryManager::fetchBookUserData(String book) {
  StaticJsonDocument<800> tempUserData = sdHandler.loadJson("/library/" + book + "/user.json");
  if (tempUserData.isNull() || tempUserData.size() == 0){
    Serial.print("Loaded empty User Data: ");
    return tempUserData;
  }
  Serial.print("Fetched Book User data: ");
  serializeJson(tempUserData, Serial);
  Serial.println();
  return tempUserData;
}

std::vector<String> LibraryManager::getLibrary() {
  return library;
}

String LibraryManager::getCurrentBook() {
  return currentBook;
}

void LibraryManager::setCurrentBook(String book){
  currentBook = book;
}

bool LibraryManager::loadCurrentBook(String book){
  currentBook = book;
  return loadBookUserData();
}
bool LibraryManager::initBookUserData(){
  JsonDocument metadata = sdHandler.loadJson("/library/" + currentBook + "/metadata.json");
  String keys[] = {"name","pages","lastPage","lastSection","title","author","isFinished"};
  String values[] = {currentBook, metadata["pages"],"0","0",metadata["title"],metadata["author"],"0"};
  sdHandler.saveJson("/library/" + currentBook + "/user.json", keys, values, NUMITEMS(keys));
  return loadBookUserData();
}

bool LibraryManager::saveBookUserData(){
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
  return sdHandler.saveJson("/library/" + currentBook + "/user.json", userData );
}

int LibraryManager::getCurrentPage(){
  if (!userData.isNull() && !userData.size() == 0){
    int currentPage = userData["lastPage"].as<int>();
    Serial.println("Getting Current page: " + String(currentPage));
    return currentPage;
  } else{
    return -1;
  }
}

int LibraryManager::getCurrentSection(){
  if (!userData.isNull() && !userData.size() == 0){
    int lastSection = userData["lastSection"].as<int>();
    Serial.println("Getting last section: " + String(lastSection));
    return lastSection;
  } else{
    return -1;
  }
}
int LibraryManager::getTotalPage(){
  if (!userData.isNull() && !userData.size() == 0){
    int totalPage = userData["pages"].as<int>();
    Serial.println("Getting Total pages: " + String(totalPage));
    return totalPage;
  } else{
    return -1;
  }
}

String LibraryManager::getTitle(){
  if (!userData.isNull() && !userData.size() == 0){
    String title = userData["title"];
    Serial.println("Getting Title: " + title);
    return title;
  } else{
    return "";
  }
}


String LibraryManager::getAuthor(){
  if (!userData.isNull() && !userData.size() == 0){
    String author = userData["author"];
    Serial.println("Getting Author: " + author);
    return author;
  } else{
    return "";
  }
}

String LibraryManager::getCurrentPageContent(int max_c){
  currentPageString = epubParser.getPageContent(getCurrentBook(), getCurrentPage());
  if (max_c > 0){
    return currentPageString.substring(0, max_c);
  } else {
    return currentPageString;
  }
  
}

String LibraryManager::getCurrentPagePath(){
  currentPagePath = epubParser.getPagePath(getCurrentBook(), getCurrentPage());
  return currentPagePath;
  
}

void LibraryManager::setCurrentPage(int page){
  if (!userData.isNull() && !userData.size() == 0){
    if (page < 0 || page >= getTotalPage()) {
      Serial.println("Invalid page index! Resetting to valid range.");
      page = 0;  // Or clamp it to a valid value
    }
    userData["lastPage"] = String(page);
    saveBookUserData();
  }
}

void LibraryManager::setCurrentSection(int section){
  if (!userData.isNull() && !userData.size() == 0){
    if (section < 0) {
      Serial.println("Invalid section index! Resetting to valid range.");
      section = 0;  // Or clamp it to a valid value
    }
    userData["lastSection"] = String(section);
    saveBookUserData();
  }
}

bool LibraryManager::nextPage(){
  int page = getCurrentPage() + 1;
  if (page < 0 || page >= getTotalPage()) {
    Serial.println("Invalid page index! Resetting to valid range.");
    page = 0;  // Or clamp it to a valid value
    setCurrentPage(page);
    return false;
  }
  setCurrentPage(page);
  return true;

}
bool LibraryManager::prevPage(){
  int page = getCurrentPage() - 1;
  if (page < 0) {
    Serial.println("Invalid page index! Resetting to valid range.");
    page = 0;
    setCurrentPage(page);
    return false;
  }
  setCurrentPage(page);
  return true;
}

bool LibraryManager::getIsFinished(){
  if (!userData.isNull() && !userData.size() == 0){
    bool isFinished = bool(userData["isFinished"].as<int>());
    Serial.println("Getting isFinished: " + isFinished);
    return isFinished;
  } else{
    return false;
  }
}

void LibraryManager::setIsFinished(bool isFinished){
  if (!userData.isNull() && !userData.size() == 0){
    userData["isFinished"] = String(isFinished);
    saveBookUserData();
  }
}