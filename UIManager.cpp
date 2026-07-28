#include "UIManager.h"

extern LibraryManager libraryManager;
extern SettingsManager settingsManager;

extern const GFXfont FreeSerif12pt7b;
extern const GFXfont FreeSerif18pt7b;
extern const GFXfont FreeSans12pt7b;
extern const GFXfont FreeSans13pt7b;
extern const GFXfont FreeSans18pt7b;
extern const GFXfont Bookerly14pt7b;
extern const GFXfont Bookerly18pt7b;
extern const GFXfont Amazon_Ember_V212pt7b;
extern const GFXfont Amazon_Ember_V213pt7b;
extern const GFXfont Amazon_Ember_V218pt7b;
extern const GFXfont Amazon_Ember_Light14pt7b;
extern const GFXfont Amazon_Ember_Light18pt7b;
static const GFXfont *FONT_PRIM_SMALL = &Amazon_Ember_V212pt7b;
static const GFXfont *FONT_PRIM_DEFAULT = &Amazon_Ember_V212pt7b;
static const GFXfont *FONT_PRIM_LARGE = &Amazon_Ember_V218pt7b;
static const GFXfont *FONT_ALT_SMALL = &FreeSans12pt7b;
static const GFXfont *FONT_ALT_DEFAULT = &FreeSans13pt7b;
static const GFXfont *FONT_ALT_LARGE = &FreeSans18pt7b;

UIManager::UIManager(Inkplate *display, bool grayscale) : display(display), imageScaler(display), currentScreen(0), currentSubScreen(0), previousScreen(0), grayscale(grayscale), scrollIndex(0), currentSection(0), currentSectionIndex(0), previousSectionIndex(0), nextSectionIndex(0), totalSections(0), pageContent(""), pagePath("") {}

void UIManager::init()
{
  display->begin();
  setFont(FONT_PRIM, FONT_SIZE_DEFAULT);
  display->setTextWrap(false); // Important for proper rendering of long text
  // display->setFullUpdateThreshold(PARTIAL_UPDATE_LIMIT_N);
  // Try touchscreen initialization up to 5 times
  bool tsInitialized = false;
  for (int i = 0; i < 5; i++)
  {
    if (display->touchscreen.init(true))
    {
      Serial.println("Touchscreen init ok");
      tsInitialized = true;
      break;
    }
    else
    {
      Serial.printf("Touchscreen init failed (attempt %d/5)\n", i + 1);
      delay(500); // Small delay before retrying
    }
  }

  if (!tsInitialized)
  {
    Serial.println("Touchscreen failed to initialize after 5 attempts.");
  }
  display->setRotation(1); // Portrait mode
  renderScreen(MAIN_SCREEN);

  snakeGame = new SnakeGame(display, [this]()
                            {
      inSnakeMode = false;
      renderScreen(currentScreen, false, true); });
}

void UIManager::setFont(uint8_t fontFamily, uint8_t fontSize)
{
  switch (fontFamily)
  {
  case FONT_PRIM:

    switch (fontSize)
    {
    case FONT_SIZE_SMALL:
      display->setFont(FONT_PRIM_SMALL);
      display->setTextSize(1);
      break;
    case FONT_SIZE_DEFAULT:
      display->setFont(FONT_PRIM_DEFAULT);
      display->setTextSize(1);
      break;
    case FONT_SIZE_LARGE:
      display->setFont(FONT_PRIM_LARGE);
      display->setTextSize(1);
      break;
    default:
      display->setFont(FONT_PRIM_DEFAULT);
      display->setTextSize(1);
      break;
    }

    break;

  case FONT_ALT:

    switch (fontSize)
    {
    case FONT_SIZE_SMALL:
      display->setFont(FONT_ALT_SMALL);
      display->setTextSize(1);
      break;
    case FONT_SIZE_DEFAULT:
      display->setFont(FONT_ALT_DEFAULT);
      display->setTextSize(1);
      break;
    case FONT_SIZE_LARGE:
      display->setFont(FONT_ALT_LARGE);
      display->setTextSize(1);
      break;
    default:
      display->setFont(FONT_ALT_DEFAULT);
      display->setTextSize(1);
      break;
    }

    break;
  default:
    display->setFont(FONT_PRIM_DEFAULT);
    display->setTextSize(1);
    break;
  }
}

// void UIManager::renderPage(const String& text) {
void UIManager::renderScreen(uint8_t s, bool partial_update, bool force)
{
  if (s == currentScreen and !force)
  {
    return;
  }
  Serial.println("Rendering Screen");
  // Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  previousScreen = currentScreen;
  currentScreen = s;

  clearDisplay();

  switch (currentScreen)
  {
  case MAIN_SCREEN:
    // renderBookList();
    renderBookGrid();
    renderMenu();
    renderMainHeader();

    break;
  case READING_SCREEN:

    renderResult = renderTextBlockSection(pageContent, BOOK_PAGE_X, BOOK_PAGE_Y, BOOK_PAGE_W, BOOK_PAGE_H, currentSection, 0);
    renderMenu();
    renderReadingHeader();

    // Serial.println(renderResult.printedText);

    break;
  case SETTINGS_SCREEN:
    renderSettings();
    renderMenu();
    renderMainHeader();
    break;
  default:
    // statements
    break;
  }
  if (partial_update)
  {
    //   Serial.println("Partial Update");
    if (partial_update_counter < PARTIAL_UPDATE_LIMIT_N)
    {
      display->partialUpdate();
      partial_update_counter++;
    }
    else
    {
      display->display();
      partial_update_counter = 0;
    }
  }
  else
  {
    display->display();
    partial_update_counter = 0;
  }
}

void UIManager::loadSectionContent()
{
  renderLoadingIcon();
  pageContent = libraryManager.getCurrentPageContent(sectionStarts[currentSection]);
  pagePath = libraryManager.getCurrentPagePath();
  // totalSections = sectionsForTextBlock(pageContent, BOOK_PAGE_X, BOOK_PAGE_Y, BOOK_PAGE_W, BOOK_PAGE_H);
  // totalSections = sectionsForTextBlock(BOOK_PAGE_X, BOOK_PAGE_Y, BOOK_PAGE_W, BOOK_PAGE_H);
  // Serial.println(pageContent);
  // Serial.println("page path is: " + pagePath);
  // Serial.println("section out of total: " + String(currentSection) + " / " + String(totalSections - 1));
}

void UIManager::loadPageSections()
{
  renderLoadingIcon();
  // pageContent = libraryManager.getCurrentPageContent(currentSectionIndex);
  // pagePath = libraryManager.getCurrentPagePath();
  // totalSections = sectionsForTextBlock(pageContent, BOOK_PAGE_X, BOOK_PAGE_Y, BOOK_PAGE_W, BOOK_PAGE_H);
  totalSections = sectionsForTextBlock(BOOK_PAGE_X, BOOK_PAGE_Y, BOOK_PAGE_W, BOOK_PAGE_H);
  // Serial.println(pageContent);
  // Serial.println("page path is: " + pagePath);
  Serial.println("section out of total: " + String(currentSection) + " / " + String(totalSections - 1));
}

uint8_t UIManager::getCurrentScreen()
{
  return currentScreen;
}

uint8_t UIManager::getPreviousScreen()
{
  return previousScreen;
}

void UIManager::flushTS(uint8_t d)
{

  uint16_t x[2], y[2];
  // delay(100);
  while (display->touchscreen.available())
  {
    Serial.println("Flushing touchscreen...");
    display->touchscreen.getData(x, y);
    delay(d);
  }
}

void UIManager::clearDisplay()
{
  if (settingsManager.getDarkMode())
  {
    // Fill framebuffer with black directly via memset (much faster than fillRect)
    if (display->getDisplayMode() == 0)
      memset(display->_partial, 0xFF, TOT_W * TOT_H / 8);
    else
      memset(display->DMemory4Bit, 0, TOT_W * TOT_H / 2);
  }
  else
  {
    display->clearDisplay();
  }
}