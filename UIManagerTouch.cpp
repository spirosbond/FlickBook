#include "UIManager.h"
#include "EpubParser.h"
#include "WebServerManager.h"

extern EpubParser epubParser;
extern SettingsManager settingsManager;
extern SDHandler sdHandler;
extern LibraryManager libraryManager;
extern WebServerManager webServerManager;

void UIManager::handleTouch()
{
    if (inSnakeMode)
    {
        snakeGame->update();
        uint8_t n;
        uint16_t x[2], y[2];
        if (display->tsAvailable() && (n = display->tsGetData(x, y)))
        {
            snakeGame->onTouch(x[0], y[0]);
        }
        return;
    }
    if (display->tsAvailable())
    {
        uint8_t n;
        uint16_t x[2], y[2];
        n = display->tsGetData(x, y);

        if (n != 0)
        {
            // Touch down or move
            Serial.printf("%d finger%c ", n, n > 1 ? 's' : '\0');
            for (uint8_t i = 0; i < n; i++)
                Serial.printf("X=%d Y=%d ", x[i], y[i]);
            Serial.println();

            if (!isTouchActive)
            {
                Serial.println("New touch start");
                // New touch start
                isTouchActive = true;
                touchStartTime = millis();
                longPressTriggered = false;
                dragTriggered = false;
                touchStartX = x[0];
                touchStartY = y[0];
            }
            else
            {
                // Touch is still active, check for long press
                unsigned long now = millis();
                int dx = lastTouchX - touchStartX;
                int dy = lastTouchY - touchStartY;
                if (abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD)
                {
                    // dragTriggered = true;
                    if (abs(dx) > abs(dy))
                    {
                        // Horizontal drag
                        if (dx > 0)
                        {
                            dragTriggered = handleDragLeft(touchStartX, touchStartY, abs(dx), abs(dy));
                        }
                        else
                        {
                            dragTriggered = handleDragRight(touchStartX, touchStartY, abs(dx), abs(dy));
                        }
                    }
                    else
                    {
                        // Vertical drag
                        if (dy > 0)
                        {
                            dragTriggered = handleDragUp(touchStartX, touchStartY, abs(dx), abs(dy));
                        }
                        else
                        {
                            dragTriggered = handleDragDown(touchStartX, touchStartY, abs(dx), abs(dy));
                        }
                    }
                }
                else if (!longPressTriggered && (now - touchStartTime > LONG_TOUCH_DUR))
                {
                    longPressTriggered = true;
                    handleLongTouch(touchStartX, touchStartY);
                }
            }
        }
        else
        {
            // Touch release
            Serial.println("Touch Release");
            if (isTouchActive)
            {
                Serial.println("Touch Release with Touch Active");
                unsigned long now = millis();
                int dx = lastTouchX - touchStartX;
                int dy = lastTouchY - touchStartY;

                if (!longPressTriggered && !dragTriggered)
                {
                    // Check for swipe
                    if (abs(dx) > SWIPE_THRESHOLD || abs(dy) > SWIPE_THRESHOLD)
                    {
                        if (abs(dx) > abs(dy))
                        {
                            // Horizontal swipe
                            if (dx > 0)
                            {
                                handleSwipeLeft(touchStartX, touchStartY);
                            }
                            else
                            {
                                handleSwipeRight(touchStartX, touchStartY);
                            }
                        }
                        else
                        {
                            // Vertical swipe
                            if (dy > 0)
                            {
                                handleSwipeUp(touchStartX, touchStartY);
                            }
                            else
                            {
                                handleSwipeDown(touchStartX, touchStartY);
                            }
                        }
                    }
                    else
                    {
                        // No swipe — treat as short touch
                        handleShortTouch(touchStartX, touchStartY);
                    }
                }
                else if (!longPressTriggered && !dragTriggered && (now - touchStartTime > LONG_TOUCH_DUR))
                {
                    longPressTriggered = true;
                    handleLongTouch(touchStartX, touchStartY);
                }
                else if (dragTriggered)
                {
                    if (PARTIAL_UPDATE_ALLOWED)
                        display->partialUpdate();
                }

                isTouchActive = false;
            }
        }

        // Store last known touch position for swipe detection
        if (n != 0)
        {
            lastTouchX = x[0];
            lastTouchY = y[0];
        }
    }
}

bool UIManager::handleShortTouch(uint16_t x, uint16_t y)
{
    Serial.printf("Short touch at X=%d Y=%d\n", x, y);
    if (handleTouchMenu(x, y, TOUCH_TYPE_SHORT))
    {
        return true; // handled
    }
    else if (handleTouchReadingHeader(x, y, TOUCH_TYPE_SHORT))
    {
        return true; // handled
    }
    else if (handleTouchMainHeader(x, y, TOUCH_TYPE_SHORT))
    {
        return true; // handled
    }
    else if (handleTouchBookList(x, y, TOUCH_TYPE_SHORT))
    {
        return true; // handled
    }
    else if (handleTouchSettingsPage(x, y, TOUCH_TYPE_SHORT))
    {
        return true; // handled
    }
    return false;
}

bool UIManager::handleLongTouch(uint16_t x, uint16_t y)
{
    // For now, just log
    Serial.printf("Long touch at X=%d Y=%d\n", x, y);
    if (handleTouchMenu(x, y, TOUCH_TYPE_LONG))
    {
        return true; // handled
    }
    // else if (handleTouchReadingHeader(x, y, TOUCH_TYPE_LONG))
    // {
    //     return true; // handled
    // }
    else if (handleTouchMainHeader(x, y, TOUCH_TYPE_LONG))
    {
        return true; // handled
    }
    else if (handleTouchBookList(x, y, TOUCH_TYPE_LONG))
    {
        return true; // handled
    }
    return false;
}

bool UIManager::handleSwipeLeft(uint16_t x, uint16_t y)
{
    Serial.println("Swipe Left");
    if (handleTouchReadingPage(x, y, TOUCH_TYPE_SLEFT))
    {
        return true; // handled
    }
    return false;
}

bool UIManager::handleSwipeRight(uint16_t x, uint16_t y)
{
    Serial.println("Swipe Right");
    if (handleTouchReadingPage(x, y, TOUCH_TYPE_SRIGHT))
    {
        return true; // handled
    }
    return false;
}

bool UIManager::handleSwipeUp(uint16_t x, uint16_t y)
{
    Serial.println("Swipe Up");
    if (handleTouchBookList(x, y, TOUCH_TYPE_SUP))
    {
        return true; // handled
    }
    else if (handleTouchReadingPage(x, y, TOUCH_TYPE_SUP))
    {
        return true; // handled
    }
    return false;
}

bool UIManager::handleSwipeDown(uint16_t x, uint16_t y)
{
    Serial.println("Swipe Down");
    if (handleTouchBookList(x, y, TOUCH_TYPE_SDOWN))
    {
        return true; // handled
    }
    else if (handleTouchReadingPage(x, y, TOUCH_TYPE_SDOWN))
    {
        return true; // handled
    }
    return false;
}

bool UIManager::handleDragLeft(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy)
{
    Serial.println("Drag Left");
    // if (handleTouchBookList(x, y, TOUCH_TYPE_DLEFT))
    // {
    //     return true; // handled
    // }
    // if (handleTouchReadingPage(x, y, TOUCH_TYPE_SLEFT))
    // {
    // handled
    // }
    return false;
}

bool UIManager::handleDragRight(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy)
{
    Serial.println("Drag Right");
    // if (handleTouchBookList(x, y, TOUCH_TYPE_DRIGHT))
    // {
    //     return true; // handled
    // }
    // if (handleTouchReadingPage(x, y, TOUCH_TYPE_SRIGHT))
    // {
    // handled
    // }
    return false;
}

bool UIManager::handleDragUp(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy)
{
    Serial.println("Drag Up");
    if (handleTouchMenu(x, y, TOUCH_TYPE_DUP, dx, dy))
    {

        return true; // handled
    }
    // else if (handleTouchBookList(x, y, TOUCH_TYPE_DUP))
    // {
    //     return true; // handled
    // }
    return false;
}

bool UIManager::handleDragDown(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy)
{
    Serial.println("Drag Down");
    if (handleTouchMenu(x, y, TOUCH_TYPE_DDOWN, dx, dy))
    {

        return true; // handled
    }
    // else if (handleTouchBookList(x, y, TOUCH_TYPE_DDOWN))
    // {
    //     return true; // handled
    // }
    return false;
}

bool UIManager::handleTouchBookList(uint16_t x, uint16_t y, uint8_t touch_type)
{
    // if (!display->tsAvailable()) return;
    // uint16_t x, y;
    // display->tsGetData(&x, &y);
    if (currentScreen == MAIN_SCREEN)
    {

        // Checking for Scrollbar arrow touches
        if (x > LIST_UP_ARROW_TOUCH[0])
        {
            // Up arrow
            if (y > LIST_UP_ARROW_TOUCH[1] and y < (LIST_UP_ARROW_TOUCH[1] + LIST_UP_ARROW_TOUCH[3]))
            {
                if (touch_type == TOUCH_TYPE_SHORT)
                {
                    int newScrollIndex = max(0, scrollIndex - 3);
                    if (newScrollIndex != scrollIndex)
                    {
                        scrollIndex = newScrollIndex;
                        renderScreen(MAIN_SCREEN, true, true);
                    }
                    flushTS(30);
                    return true;
                }
                // Down arrow
            }
            else if (y > LIST_DOWN_ARROW_TOUCH[1] and y < (LIST_DOWN_ARROW_TOUCH[1] + LIST_DOWN_ARROW_TOUCH[3]))
            {
                if (touch_type == TOUCH_TYPE_SHORT)
                {
                    std::vector<BookInfo> bookList = libraryManager.getLibrary();
                    int newScrollIndex = min((int)bookList.size() - LIST_MAX_FILES, scrollIndex + 3);
                    if (newScrollIndex != scrollIndex)
                    {
                        scrollIndex = newScrollIndex;
                        renderScreen(MAIN_SCREEN, true, true);
                    }
                    flushTS(30);
                    return true;
                }
            }

            // Checking for list item touches
        }
        else if (x > LIST_START_X and x < (LIST_START_X + LIST_WIDTH))
        {
            if (y > LIST_START_Y and y < (LIST_START_Y + (LIST_MAX_FILES + 1) * LIST_ITEM_HEIGHT))
            {
                int index = (y - LIST_START_Y) / LIST_ITEM_HEIGHT;
                std::vector<BookInfo> bookList = libraryManager.getLibrary();
                if (index >= 0 && index < LIST_MAX_FILES && (scrollIndex + index) < bookList.size())
                {

                    if (touch_type == TOUCH_TYPE_SHORT)
                    {
                        Serial.print("Selected file: ");
                        Serial.println(bookList[scrollIndex + index].name);
                        flushTS(30);
                        libraryManager.loadCurrentBook(bookList[scrollIndex + index].name);
                        currentSection = libraryManager.getCurrentSection();
                        // currentSectionIndex = libraryManager.getCurrentSectionIndex();
                        // nextSectionIndex = currentSectionIndex;
                        loadPageSections();
                        loadSectionContent();
                        renderScreen(READING_SCREEN);
                        return true;
                    }
                    else if (touch_type == TOUCH_TYPE_LONG)
                    {
                        Serial.print("Long pressed: ");
                        Serial.println(bookList[scrollIndex + index].name);
                        flushTS(30);
                        libraryManager.loadCurrentBook(bookList[scrollIndex + index].name);
                        // currentSection = libraryManager.getCurrentSection();
                        // currentSectionIndex = libraryManager.getCurrentSectionIndex();
                        // nextSectionIndex = currentSectionIndex;
                        // loadPageSections();
                        // loadSectionContent();
                        libraryManager.setIsFinished(!libraryManager.getIsFinished());
                        renderScreen(MAIN_SCREEN, true, true);
                        return true;
                    }
                    else if (touch_type == TOUCH_TYPE_SDOWN)
                    {
                        int newScrollIndex = min((int)bookList.size() - LIST_MAX_FILES, scrollIndex + 3);
                        if (newScrollIndex != scrollIndex)
                        {
                            scrollIndex = newScrollIndex;
                            renderScreen(MAIN_SCREEN, true, true);
                        }
                        flushTS(30);
                        return true;
                    }
                    else if (touch_type == TOUCH_TYPE_SUP)
                    {
                        int newScrollIndex = max(0, scrollIndex - 3);
                        if (newScrollIndex != scrollIndex)
                        {
                            scrollIndex = newScrollIndex;
                            renderScreen(MAIN_SCREEN, true, true);
                        }
                        flushTS(30);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool UIManager::handleTouchMenu(uint16_t x, uint16_t y, uint8_t touch_type, uint16_t dx, uint16_t dy)
{
    if ((x >= MENU_ITEM_HOME[0]) and (y >= MENU_ITEM_HOME[1]) and (x <= (MENU_ITEM_HOME[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_HOME[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: HOME");
            renderScreen(MAIN_SCREEN);
            return true;
        }
    }
    else if ((currentScreen == READING_SCREEN) and (x >= MENU_ITEM_START[0]) and (y >= MENU_ITEM_START[1]) and (x <= (MENU_ITEM_START[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_START[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: START");
            libraryManager.setCurrentPage(0);
            currentSection = 0;
            // previousSectionIndex = 0;
            // currentSectionIndex = 0;
            libraryManager.setCurrentSection(currentSection);
            // libraryManager.setCurrentSectionIndex(currentSectionIndex);
            loadPageSections();
            loadSectionContent();
            renderScreen(READING_SCREEN, false, true);
            flushTS(50);
            return true;
        }
    }
    else if ((x >= MENU_ITEM_BACKLIGHT[0]) and (y >= MENU_ITEM_BACKLIGHT[1]) and (x <= (MENU_ITEM_BACKLIGHT[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_BACKLIGHT[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: BACKLIGHT");
            settingsManager.incBacklight();
            if (currentScreen == SETTINGS_SCREEN)
            {
                renderScreen(SETTINGS_SCREEN, true, true);
            }
            else
            {
                renderMenu(true);
            }
            // delay(100);
            flushTS(50);
            return true;
        }
        else if (touch_type == TOUCH_TYPE_LONG)
        {
            Serial.println("Long Touched Menu Item: BACKLIGHT");
            settingsManager.setBacklight(0, true);
            renderMenu(true);
            // delay(100);
            // flushTS(50); DONT FLUSH AFTER LONG TOUCH TO ALLOW FINGER RELEASE DETECTION
            return true;
        }
        else if (touch_type == TOUCH_TYPE_DUP)
        {
            Serial.println("Drag UP Touched Menu Item: BACKLIGHT dy: " + String(dy));
            settingsManager.setBacklight(dy / 10, true);
            drawBacklightIndicator();
            // renderMenu(true);
            // delay(100);
            // flushTS(50); DONT FLUSH AFTER DRAG TOUCH TO ALLOW DX DY DETECTION
            return true;
        }
        else if (touch_type == TOUCH_TYPE_DDOWN)
        {
            Serial.println("Drag DOWN Touched Menu Item: BACKLIGHT dy: " + String(dy));
            settingsManager.setBacklight(dy / 10, true);
            drawBacklightIndicator();
            // renderMenu(true);
            // delay(100);
            // flushTS(50); DONT FLUSH AFTER DRAG TOUCH TO ALLOW DX DY DETECTION
            return true;
        }
    }
    else if ((currentScreen == MAIN_SCREEN) and (x >= MENU_ITEM_SHOW_FINISHED[0]) and (y >= MENU_ITEM_SHOW_FINISHED[1]) and (x <= (MENU_ITEM_SHOW_FINISHED[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_SHOW_FINISHED[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: SHOW_FINISHED");
            libraryManager.setShowFinishedBooks(!libraryManager.getShowFinishedBooks());
            scrollIndex = 0;
            renderScreen(MAIN_SCREEN, true, true);
            flushTS(50);
            return true;
        }
    }
    else if ((currentScreen == READING_SCREEN) and (x >= MENU_ITEM_BACK[0]) and (y >= MENU_ITEM_BACK[1]) and (x <= (MENU_ITEM_BACK[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_BACK[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: BACK");
            if (libraryManager.prevPage())
            {
                currentSection = 0;
                // previousSectionIndex = 0;
                // currentSectionIndex = 0;
                libraryManager.setCurrentSection(currentSection);
                // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                loadPageSections();
                loadSectionContent();
                renderScreen(READING_SCREEN, true, true);
            }
            flushTS(50);
            return true;
        }
    }
    else if ((currentScreen == READING_SCREEN) and (x >= MENU_ITEM_FW[0]) and (y >= MENU_ITEM_FW[1]) and (x <= (MENU_ITEM_FW[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_FW[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: FW");
            if (libraryManager.nextPage())
            {
                currentSection = 0;
                // previousSectionIndex = 0;
                // currentSectionIndex = 0;
                libraryManager.setCurrentSection(currentSection);
                // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                loadPageSections();
                loadSectionContent();
                renderScreen(READING_SCREEN, true, true);
            }
            flushTS(50);
            return true;
        }
    }
    else if ((currentScreen == READING_SCREEN) and (x >= MENU_ITEM_UP[0]) and (y >= MENU_ITEM_UP[1]) and (x <= (MENU_ITEM_UP[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_UP[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: UP");
            currentSection--;
            // previousSectionIndex = currentSectionIndex; TODO
            // currentSectionIndex = previousSectionIndex;
            if (currentSection < 0)
            {
                currentSection = 0;
            }
            else
            {
                loadSectionContent();
                renderScreen(READING_SCREEN, true, true);
            }
            libraryManager.setCurrentSection(currentSection);
            // libraryManager.setCurrentSectionIndex(currentSectionIndex);

            flushTS(50);
            return true;
        }
    }
    else if ((currentScreen == READING_SCREEN) and (x >= MENU_ITEM_DOWN[0]) and (y >= MENU_ITEM_DOWN[1]) and (x <= (MENU_ITEM_DOWN[0] + MENU_ITEM_SIZE)) and (y <= (MENU_ITEM_DOWN[1] + MENU_ITEM_SIZE)))
    {
        if (touch_type == TOUCH_TYPE_SHORT)
        {
            Serial.println("Touched Menu Item: DOWN");
            currentSection++;
            // previousSectionIndex = currentSectionIndex;
            // currentSectionIndex = nextSectionIndex;
            if (currentSection >= totalSections)
            {
                currentSection = totalSections - 1;
            }
            else
            {
                loadSectionContent();
                renderScreen(READING_SCREEN, true, true);
            }
            libraryManager.setCurrentSection(currentSection);
            // libraryManager.setCurrentSectionIndex(currentSectionIndex);
            flushTS(50);
            return true;
        }
    }
    return false;
}

bool UIManager::handleTouchReadingHeader(uint16_t x, uint16_t y, uint8_t touch_type)
{
    if (currentScreen == READING_SCREEN)
    {
        if ((x >= READING_HEADER_ITEM_RELOAD[0]) and (y >= READING_HEADER_ITEM_RELOAD[1]) and (x <= (READING_HEADER_ITEM_RELOAD[0] + HEADER_ITEM_SIZE)) and (y <= (READING_HEADER_ITEM_RELOAD[1] + HEADER_ITEM_SIZE)))
        {
            if (touch_type == TOUCH_TYPE_SHORT)
            {
                Serial.println("Touched Header Item: RELOAD");
                epubParser.parseEpubMetadata(libraryManager.getCurrentBook());
                libraryManager.initBookUserData();
                libraryManager.initCurrentPageSections();
                currentSection = 0;
                // previousSectionIndex = 0;
                // currentSectionIndex = 0;
                libraryManager.setCurrentSection(currentSection);
                // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                loadPageSections();
                loadSectionContent();
                renderScreen(READING_SCREEN, false, true);
                // delay(100);
                flushTS(50);
                return true;
            }
        }
    }
    return false;
}

bool UIManager::handleTouchReadingPage(uint16_t x, uint16_t y, uint8_t touch_type)
{
    if (currentScreen == READING_SCREEN)
    {
        if ((x >= BOOK_PAGE_X) and (y >= BOOK_PAGE_Y) and (y <= BOOK_PAGE_Y + BOOK_PAGE_H))
        {
            if ((touch_type == TOUCH_TYPE_SDOWN) and (settingsManager.getGestures()))
            {
                Serial.println("Touched Reading Page: Swiped Down");
                currentSection++;
                // previousSectionIndex = currentSectionIndex;
                // currentSectionIndex = nextSectionIndex;
                if (currentSection >= totalSections)
                {
                    currentSection = totalSections - 1;
                }
                else
                {
                    loadSectionContent();
                    renderScreen(READING_SCREEN, true, true);
                }
                libraryManager.setCurrentSection(currentSection);
                // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                flushTS(50);

                return true;
            }
            else if ((touch_type == TOUCH_TYPE_SUP) and (settingsManager.getGestures()))
            {
                Serial.println("Touched Reading Page: Swiped Up");
                currentSection--;
                // previousSectionIndex = currentSectionIndex; TODO
                // currentSectionIndex = previousSectionIndex;
                if (currentSection < 0)
                {
                    currentSection = 0;
                }
                else
                {
                    loadSectionContent();
                    renderScreen(READING_SCREEN, true, true);
                }
                libraryManager.setCurrentSection(currentSection);
                // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                flushTS(50);

                return true;
            }
            else if ((touch_type == TOUCH_TYPE_SLEFT) and (settingsManager.getGestures()))
            {
                Serial.println("Touched Reading Page: Swiped Left");
                if (libraryManager.prevPage())
                {
                    currentSection = 0;
                    // previousSectionIndex = 0;
                    // currentSectionIndex = 0;
                    libraryManager.setCurrentSection(currentSection);
                    // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                    loadPageSections();
                    loadSectionContent();
                    renderScreen(READING_SCREEN, true, true);
                }
                flushTS(50);

                return true;
            }
            else if ((touch_type == TOUCH_TYPE_SRIGHT) and (settingsManager.getGestures()))
            {
                Serial.println("Touched Reading Page: Swiped Right");
                if (libraryManager.nextPage())
                {
                    currentSection = 0;
                    // previousSectionIndex = 0;
                    // currentSectionIndex = 0;
                    libraryManager.setCurrentSection(currentSection);
                    // libraryManager.setCurrentSectionIndex(currentSectionIndex);
                    loadPageSections();
                    loadSectionContent();
                    renderScreen(READING_SCREEN, true, true);
                }
                flushTS(50);

                return true;
            }
        }
    }
    return false;
}

bool UIManager::handleTouchMainHeader(uint16_t x, uint16_t y, uint8_t touch_type)
{
    if (currentScreen == MAIN_SCREEN)
    {
        if ((x >= MAIN_HEADER_ITEM_SETTINGS[0]) and (y >= MAIN_HEADER_ITEM_SETTINGS[1]) and (x <= (MAIN_HEADER_ITEM_SETTINGS[0] + HEADER_ITEM_SIZE)) and (y <= (MAIN_HEADER_ITEM_SETTINGS[1] + HEADER_ITEM_SIZE)))
        {
            if (touch_type == TOUCH_TYPE_SHORT)
            {
                Serial.println("Touched Header Item Settings: Short Touch");
                renderScreen(SETTINGS_SCREEN);
                flushTS(50);
                return true;
            }
            else if (touch_type == TOUCH_TYPE_LONG)
            {
                Serial.println("Touched Header Item Settings: Long Touch");
                inSnakeMode = true;
                snakeGame->begin(settingsManager.getFgColor(), settingsManager.getBgColor());
                return true;
            }
        }
    }
    return false;
}

bool UIManager::handleTouchSettingsPage(uint16_t x, uint16_t y, uint8_t touch_type)
{
    if (currentScreen == SETTINGS_SCREEN)
    {
        if ((x >= SETTINGS_TAB_1[0]) and (y >= SETTINGS_TAB_1[1]) and (x <= (SETTINGS_TAB_1[0] + SETTINGS_TAB_1[2])) and (y <= (SETTINGS_TAB_1[1] + SETTINGS_TAB_1[3])))
        {
            if (touch_type == TOUCH_TYPE_SHORT)
            {
                Serial.println("Touched Settings Tab Gestures: TAB 1");
                currentSubScreen = SETTINGS_TAB_1_ID;
                renderScreen(SETTINGS_SCREEN, true, true);
                flushTS(50);
                return true;
            }
        }
        else if ((x >= SETTINGS_TAB_2[0]) and (y >= SETTINGS_TAB_2[1]) and (x <= (SETTINGS_TAB_2[0] + SETTINGS_TAB_2[2])) and (y <= (SETTINGS_TAB_2[1] + SETTINGS_TAB_2[3])))
        {
            if (touch_type == TOUCH_TYPE_SHORT)
            {
                Serial.println("Touched Settings Tab Gestures: TAB 2");
                currentSubScreen = SETTINGS_TAB_2_ID;
                renderScreen(SETTINGS_SCREEN, true, true);
                flushTS(50);
                return true;
            }
        }
        else if ((x >= SETTINGS_TAB_3[0]) and (y >= SETTINGS_TAB_3[1]) and (x <= (SETTINGS_TAB_3[0] + SETTINGS_TAB_3[2])) and (y <= (SETTINGS_TAB_3[1] + SETTINGS_TAB_3[3])))
        {
            if (touch_type == TOUCH_TYPE_SHORT)
            {
                Serial.println("Touched Settings Tab Gestures: TAB 3");
                currentSubScreen = SETTINGS_TAB_3_ID;
                renderScreen(SETTINGS_SCREEN, true, true);
                flushTS(50);
                return true;
            }
        }
        if (currentSubScreen == SETTINGS_TAB_1_ID)
        {
            if ((x >= (SETTINGS_PAGE_X + SETTINGS_PAGE_W - 70)) and (y >= (SETTINGS_ITEM_2[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2)) and (x <= (SETTINGS_PAGE_X + SETTINGS_PAGE_W - 70 + SETTINGS_ITEM_STATUS_ICON_SIZE)) and (y <= (SETTINGS_ITEM_2[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2 + SETTINGS_ITEM_STATUS_ICON_SIZE)))
            {
                if (touch_type == TOUCH_TYPE_SHORT)
                {
                    Serial.println("Touched Settings Item Gestures: Short Touch");
                    settingsManager.setGestures(!settingsManager.getGestures());
                    renderScreen(SETTINGS_SCREEN, true, true);
                    flushTS(50);
                    return true;
                }
            }
            else if ((x >= (SETTINGS_PAGE_X + SETTINGS_PAGE_W - 70)) and (y >= (SETTINGS_ITEM_3[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2)) and (x <= (SETTINGS_PAGE_X + SETTINGS_PAGE_W - 70 + SETTINGS_ITEM_STATUS_ICON_SIZE)) and (y <= (SETTINGS_ITEM_3[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2 + SETTINGS_ITEM_STATUS_ICON_SIZE)))
            {
                if (touch_type == TOUCH_TYPE_SHORT)
                {
                    Serial.println("Touched Settings Item DarkMode: Short Touch");
                    settingsManager.setDarkMode(!settingsManager.getDarkMode());
                    renderScreen(SETTINGS_SCREEN, false, true);
                    flushTS(50);
                    return true;
                }
            }
        }
        else if (currentSubScreen == SETTINGS_TAB_3_ID)
        {
            if ((x >= (SETTINGS_PAGE_X + SETTINGS_PAGE_W - 70)) and (y >= (SETTINGS_ITEM_1[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2)) and (x <= (SETTINGS_PAGE_X + SETTINGS_PAGE_W - 70 + SETTINGS_ITEM_STATUS_ICON_SIZE)) and (y <= (SETTINGS_ITEM_1[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2 + SETTINGS_ITEM_STATUS_ICON_SIZE)))
            {
                if (touch_type == TOUCH_TYPE_SHORT)
                {
                    Serial.println("Touched Settings Item WebServer: Short Touch");
                    settingsManager.setWebserver(!settingsManager.getWebserver());
                    if (settingsManager.getWebserver())
                    {
                        webServerManager.start();
                    }
                    else
                    {
                        webServerManager.stop();
                    }
                    renderScreen(SETTINGS_SCREEN, true, true);
                    flushTS(50);
                    return true;
                }
            }
        }
    }

    return false;
}