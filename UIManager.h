#ifndef UIMANAGER_H
#define UIMANAGER_H
#include "Inkplate.h"
#include "battSymbol.h"
#include "Fonts/FreeSerif12pt7b.h"
#include "Fonts/FreeSerif18pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSans18pt7b.h"
#include "SnakeGame.h"
#define FONT_SERIF 0
#define FONT_SANS 1
#include "SettingsManager.h"
#include "LibraryManager.h"
#include "SDHandler.h"
// #include "libs/TJpg_Decoder.h"
// #include "EpubParser.h"
#define ICONS_FOLDER "/assets/icons/"
#define ICON_BATTERY "battery_empty_icon.png"
#define ICON_BATTERY_W 70
#if GRAYSCALE == 1
#define WHITE 65535
#define BLACK 0
#define PARTIAL_UPDATE_ALLOWED 0
#define PARTIAL_UPDATE_LIMIT_N 0
#else
#define WHITE 0
#define BLACK 1
#define PARTIAL_UPDATE_ALLOWED 1
#define PARTIAL_UPDATE_LIMIT_N 7
#endif
#define TOUCH_TYPE_SHORT 1
#define TOUCH_TYPE_LONG 2
#define TOUCH_TYPE_SUP 3
#define TOUCH_TYPE_SDOWN 4
#define TOUCH_TYPE_SLEFT 5
#define TOUCH_TYPE_SRIGHT 6
#define LONG_TOUCH_DUR 800
#define SWIPE_THRESHOLD 60    // Minimum pixels moved to be considered a swipe
#define FONT_SIZE_SMALL 1
#define FONT_SIZE_DEFAULT 2
#define FONT_SIZE_LARGE 3
#define FONT_SIZE_SMALL_PX 16
#define FONT_SIZE_DEFAULT_PX 16
#define FONT_SIZE_LARGE_PX 24
#define NORMAL_LINE_HEIGHT 27 // 3 * 9
#define HEADER_LINE_HEIGHT 36 // 4 * 9
#define MAIN_SCREEN 1
#define READING_SCREEN 2
#define SETTINGS_SCREEN 3
#define TOT_W 768
#define TOT_H 1024
#define MENU_ITEM_SIZE 80
#define MENU_ITEM_RADIUS 15
#define MENU_ICON_SIZE 48
const int MENU_ITEM_HOME[] = {2, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
const int MENU_ITEM_BACKLIGHT[] = {1*MENU_ITEM_SIZE+1, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
const int MENU_ITEM_START[] = {3*MENU_ITEM_SIZE+1, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
const int MENU_ITEM_BACK[] = {4*MENU_ITEM_SIZE+1, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
const int MENU_ITEM_FW[] = {5*MENU_ITEM_SIZE+1, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
const int MENU_ITEM_UP[] = {TOT_W-2*MENU_ITEM_SIZE-10, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
const int MENU_ITEM_DOWN[] = {TOT_W-MENU_ITEM_SIZE-10, TOT_H-MENU_ITEM_SIZE-1, MENU_ITEM_SIZE-2, MENU_ITEM_SIZE-1};
// const int MENU_ITEM_RELOAD[] = {6*MENU_ITEM_SIZE,TOT_H-MENU_ITEM_SIZE,MENU_ITEM_SIZE-1,MENU_ITEM_SIZE};

const String MENU_ITEM_ICON[] = {"home_ic_icon","brightness_ic_settings_icon","start_ic_icon","backward_ic_icon","forward_ic_icon","arrow_ic_up_icon","arrow_ic_down_icon"};

#define LIST_SCROLLBAR_WIDTH 20
#define LIST_ARROW_SIZE 30
#define LIST_ARROW_RADIUS 5
#define LIST_ARROW_TOUCH_PADDING_X 3
#define LIST_ARROW_TOUCH_PADDING_Y 1
#define LIST_START_X (10)
#define LIST_START_Y (150)
#define LIST_TITLE_X (LIST_START_X)
#define LIST_TITLE_Y (LIST_START_Y-30)
#define LIST_WIDTH (TOT_W - 4*LIST_SCROLLBAR_WIDTH)
#define LIST_ITEM_HEIGHT 100
#define LIST_ITEM_MAX_CHARS 45
const int LIST_UP_ARROW_DRAW[][2] = { {TOT_W - 2.5*LIST_SCROLLBAR_WIDTH + LIST_ARROW_SIZE/2, LIST_START_Y - LIST_ARROW_SIZE},
                                      {TOT_W - 10 - 0.5*LIST_SCROLLBAR_WIDTH, LIST_START_Y - 4},
                                      {TOT_W - 1.5*LIST_SCROLLBAR_WIDTH - 20, LIST_START_Y - 4}};
const int LIST_UP_ARROW_TOUCH[] =   {TOT_W - 2.5*LIST_SCROLLBAR_WIDTH - LIST_ARROW_TOUCH_PADDING_X,
                                    LIST_START_Y - LIST_ARROW_SIZE - LIST_ARROW_TOUCH_PADDING_Y,
                                    LIST_ARROW_SIZE + 2*LIST_ARROW_TOUCH_PADDING_X,
                                    LIST_ARROW_SIZE + 2*LIST_ARROW_TOUCH_PADDING_Y};

const int LIST_DOWN_ARROW_DRAW[][2] = { {TOT_W - 2.5*LIST_SCROLLBAR_WIDTH + LIST_ARROW_SIZE/2, TOT_H - MENU_ITEM_SIZE - LIST_ARROW_SIZE},
                                      {TOT_W - 10 - 0.5*LIST_SCROLLBAR_WIDTH, TOT_H - MENU_ITEM_SIZE - 2*LIST_ARROW_SIZE + 4},
                                      {TOT_W - 1.5*LIST_SCROLLBAR_WIDTH - 20,  TOT_H - MENU_ITEM_SIZE - 2*LIST_ARROW_SIZE + 4}};
const int LIST_DOWN_ARROW_TOUCH[] = { TOT_W - 2.5*LIST_SCROLLBAR_WIDTH - LIST_ARROW_TOUCH_PADDING_X,
                                      TOT_H - MENU_ITEM_SIZE - 2*LIST_ARROW_SIZE - LIST_ARROW_TOUCH_PADDING_Y,
                                      LIST_ARROW_SIZE + 2*LIST_ARROW_TOUCH_PADDING_X,
                                      LIST_ARROW_SIZE + 2*LIST_ARROW_TOUCH_PADDING_Y};
#define BOOK_PAGE_X (50)
#define BOOK_PAGE_Y (110)
#define BOOK_PAGE_W (TOT_W - 2*BOOK_PAGE_X)
#define BOOK_PAGE_H (TOT_H - MENU_ITEM_SIZE - BOOK_PAGE_Y)
#define HEADER_H (60)
const int HEADER[] = {0, 0, TOT_W, HEADER_H};
#define HEADER_ITEM_SIZE HEADER_H
#define HEADER_ITEM_RADIUS 15
#define HEADER_ICON_SIZE 48
const int MAIN_HEADER_ITEM_SETTINGS[] = {TOT_W-HEADER_ITEM_SIZE-ICON_BATTERY_W-5, 0, HEADER_ITEM_SIZE-1, HEADER_ITEM_SIZE};
const String MAIN_HEADER_ITEM_ICON[] = {"settings_icon"};
const int READING_HEADER_ITEM_RELOAD[] = {TOT_W-HEADER_ITEM_SIZE-ICON_BATTERY_W-5, 0, HEADER_ITEM_SIZE-1, HEADER_ITEM_SIZE};
const String READING_HEADER_ITEM_ICON[] = {"reload_ic_icon"};
const int HEADER_ITEM_LOADING[] = {TOT_W-2*HEADER_ITEM_SIZE-ICON_BATTERY_W-5, 0, HEADER_ITEM_SIZE-1, HEADER_ITEM_SIZE};
const String HEADER_ITEM_LOADING_ICON = "loading_icon";

const uint8_t LIST_MAX_FILES = (TOT_H-LIST_START_Y-MENU_ICON_SIZE-HEADER_H) / LIST_ITEM_HEIGHT; // Number of files visible at a time in the list
// const uint8_t LIST_MAX_FILES = 7;

#define SETTINGS_TAB_X (50)
#define SETTINGS_TAB_Y (100)
#define SETTINGS_TAB_SIZE (100)
const int SETTINGS_TAB_1[] = {SETTINGS_TAB_X, SETTINGS_TAB_Y, SETTINGS_TAB_SIZE, SETTINGS_TAB_SIZE};
const int SETTINGS_TAB_2[] = {SETTINGS_TAB_X + SETTINGS_TAB_SIZE, SETTINGS_TAB_Y, SETTINGS_TAB_SIZE, SETTINGS_TAB_SIZE};
const int SETTINGS_TAB_3[] = {SETTINGS_TAB_X + 2*SETTINGS_TAB_SIZE, SETTINGS_TAB_Y, SETTINGS_TAB_SIZE, SETTINGS_TAB_SIZE};
#define SETTINGS_TAB_ICON_SIZE 48
const String SETTINGS_TAB_ICON[] = {"eye_icon","gesture_icon","cloud_icon"};
#define SETTINGS_PAGE_X (SETTINGS_TAB_X)
#define SETTINGS_PAGE_Y (SETTINGS_TAB_Y + SETTINGS_TAB_SIZE)
#define SETTINGS_PAGE_W (TOT_W - 2*SETTINGS_PAGE_X)
#define SETTINGS_PAGE_H (TOT_H - SETTINGS_PAGE_Y - MENU_ITEM_SIZE - 30)
#define SETTINGS_ITEM_X (SETTINGS_PAGE_X + 10)
#define SETTINGS_ITEM_Y (70)
#define SETTINGS_ITEM_SIZE (100)
const int SETTINGS_ITEM_1[] = {SETTINGS_ITEM_X, SETTINGS_TAB_SIZE + SETTINGS_ITEM_SIZE + SETTINGS_ITEM_Y};
const int SETTINGS_ITEM_2[] = {SETTINGS_ITEM_X, SETTINGS_TAB_SIZE + 2*SETTINGS_ITEM_SIZE + SETTINGS_ITEM_Y};
const int SETTINGS_ITEM_3[] = {SETTINGS_ITEM_X, SETTINGS_TAB_SIZE + 3*SETTINGS_ITEM_SIZE + SETTINGS_ITEM_Y};
#define SETTINGS_ITEM_STATUS_ICON_SIZE 48
const String SETTINGS_ITEM_STATUS_ICON[] = {"circle_blank_icon","circle_check_icon"};
#define SETTINGS_SEP_X1 (SETTINGS_ITEM_X)
#define SETTINGS_SEP_X2 (SETTINGS_SEP_X1 + SETTINGS_PAGE_W - 2*10)
#define SETTINGS_SEP_THICKNESS (1)
const int SETTINGS_SEP_1[] = {SETTINGS_SEP_X1, SETTINGS_TAB_Y + SETTINGS_TAB_SIZE + SETTINGS_ITEM_SIZE, SETTINGS_SEP_X2, SETTINGS_TAB_Y + SETTINGS_TAB_SIZE + SETTINGS_ITEM_SIZE, BLACK, SETTINGS_SEP_THICKNESS};
const int SETTINGS_SEP_2[] = {SETTINGS_SEP_X1, SETTINGS_TAB_Y + SETTINGS_TAB_SIZE + 2*SETTINGS_ITEM_SIZE, SETTINGS_SEP_X2, SETTINGS_TAB_Y + SETTINGS_TAB_SIZE + 2*SETTINGS_ITEM_SIZE, BLACK, SETTINGS_SEP_THICKNESS};
#define LOADING_MSG_W (200)
#define LOADING_MSG_H (80)
#define LOADING_MSG_X ((TOT_W - LOADING_MSG_W) / 2)
#define LOADING_MSG_Y ((TOT_H - LOADING_MSG_H) / 2)

struct TextRenderResult {
    String printedText = "";
    size_t startIndex = 0;
    size_t endIndex = 0;
};

class UIManager {
public:
    UIManager(Inkplate* display, bool grayscale);
    void init();
    void handleTouch();
    void handleShortTouch(uint16_t x, uint16_t y);
    void handleLongTouch(uint16_t x, uint16_t y);
    void handleSwipeLeft(uint16_t x, uint16_t y);
    void handleSwipeRight(uint16_t x, uint16_t y);
    void handleSwipeUp(uint16_t x, uint16_t y);
    void handleSwipeDown(uint16_t x, uint16_t y);
    // void renderPage(const String& text);
    void renderScreen(uint8_t s, bool partial_update = false, bool force = false);
    void loadPageContent();
    uint8_t getCurrentScreen();
    uint8_t getPreviousScreen();
    void renderMenu(bool partial_update = false);
    void renderReadingHeader(bool partial_update = false);
    void renderMainHeader(bool partial_update = false);
    void flushTS(uint8_t d);
    // void loadBookList();
    void renderBookList(bool partial_update = false);
    bool handleTouchBookList(uint16_t x, uint16_t y, uint8_t touch_type);
    bool handleTouchMenu(uint16_t x, uint16_t y, uint8_t touch_type);
    bool handleTouchMainHeader(uint16_t x, uint16_t y, uint8_t touch_type);
    bool handleTouchReadingHeader(uint16_t x, uint16_t y, uint8_t touch_type);
    bool handleTouchReadingPage(uint16_t x, uint16_t y, uint8_t touch_type);
    void drawBattery(bool invert = 0);
    void renderLoadingMsg(const String& message = "Loading...", bool partial_update = PARTIAL_UPDATE_ALLOWED);
    void renderLoadingIcon(bool partial_update = PARTIAL_UPDATE_ALLOWED);
    bool drawIcon(String iconName, int x, int y, bool invert = 0);
    // void renderTextBlockSection(const String &text, int startX, int startY, int width, int height, int pageNum);
    TextRenderResult renderTextBlockSection(const String &text, int startX, int startY, int width, int height, int pageNum, int startIdx);
    int sectionsForTextBlock(const String &text, int startX, int startY, int width, int height);
    void renderImage(const String &imgPath, int x, int y, bool invert = 0);
    void setFont(uint8_t fontFamily,uint8_t fontSize);
    void processTextBlock(
        const String &text, int startX, int startY, int width, int height,
        std::function<void(const String&, int, int, int)> onPrintLine,
        std::function<void(const String&, int, int, int, int)> onRenderImage,
        std::function<void()> onPageBreak,
        int startIdx = 0
    );
    void renderSettings(bool partial_update = false);
private:
    Inkplate* display;
    uint8_t currentScreen;
    uint8_t previousScreen;
    bool grayscale;
    int scrollIndex;
    String pageContent;
    String pagePath;
    int currentSection;
    int totalSections;
    bool isTouchActive = false;
    unsigned long touchStartTime = 0;
    bool longPressTriggered = false;
    uint16_t lastTouchX = 0, lastTouchY = 0, touchStartX = 0, touchStartY = 0;
    uint8_t partial_update_counter = 0;
    TextRenderResult renderResult;
    bool inSnakeMode = false;
    SnakeGame* snakeGame = nullptr;
};
#endif