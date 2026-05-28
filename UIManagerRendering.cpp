#include "UIManager.h"
extern "C"
{
#include "libs/qrcode.c"
}
extern SettingsManager settingsManager;
extern SDHandler sdHandler;
extern LibraryManager libraryManager;
extern WebServerManager webServerManager;

void UIManager::renderMenu(bool partial_update)
{

    display->fillRoundRect(MENU_ITEM_HOME[0], MENU_ITEM_HOME[1], MENU_ITEM_HOME[2], MENU_ITEM_HOME[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
    display->drawRoundRect(MENU_ITEM_HOME[0], MENU_ITEM_HOME[1], MENU_ITEM_HOME[2], MENU_ITEM_HOME[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
    drawIcon(MENU_ITEM_ICON[0], MENU_ITEM_HOME[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_HOME[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);

    if (GRAYSCALE)
    {
        display->fillRoundRect(MENU_ITEM_BACKLIGHT[0], MENU_ITEM_BACKLIGHT[1], MENU_ITEM_BACKLIGHT[2], MENU_ITEM_BACKLIGHT[3], MENU_ITEM_RADIUS, settingsManager.getBacklight() / 10); // Arguments are: start X, start Y, size X, size Y, radius, color (if grayscale 0-6)
        drawIcon(MENU_ITEM_ICON[1], MENU_ITEM_BACKLIGHT[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_BACKLIGHT[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, settingsManager.getBacklight() < 30);
    }
    else
    {
        display->fillRoundRect(MENU_ITEM_BACKLIGHT[0], MENU_ITEM_BACKLIGHT[1], MENU_ITEM_BACKLIGHT[2], MENU_ITEM_BACKLIGHT[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
        display->drawRoundRect(MENU_ITEM_BACKLIGHT[0], MENU_ITEM_BACKLIGHT[1], MENU_ITEM_BACKLIGHT[2], MENU_ITEM_BACKLIGHT[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color

        drawBacklightIndicator();

        drawIcon(MENU_ITEM_ICON[1], MENU_ITEM_BACKLIGHT[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_BACKLIGHT[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);
    }

    if (currentScreen == READING_SCREEN)
    {
        display->fillRoundRect(MENU_ITEM_START[0], MENU_ITEM_START[1], MENU_ITEM_START[2], MENU_ITEM_START[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
        display->drawRoundRect(MENU_ITEM_START[0], MENU_ITEM_START[1], MENU_ITEM_START[2], MENU_ITEM_START[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
        drawIcon(MENU_ITEM_ICON[2], MENU_ITEM_START[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_START[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);

        display->fillRoundRect(MENU_ITEM_BACK[0], MENU_ITEM_BACK[1], MENU_ITEM_BACK[2], MENU_ITEM_BACK[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
        display->drawRoundRect(MENU_ITEM_BACK[0], MENU_ITEM_BACK[1], MENU_ITEM_BACK[2], MENU_ITEM_BACK[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
        drawIcon(MENU_ITEM_ICON[3], MENU_ITEM_BACK[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_BACK[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);

        display->fillRoundRect(MENU_ITEM_FW[0], MENU_ITEM_FW[1], MENU_ITEM_FW[2], MENU_ITEM_FW[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
        display->drawRoundRect(MENU_ITEM_FW[0], MENU_ITEM_FW[1], MENU_ITEM_FW[2], MENU_ITEM_FW[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
        drawIcon(MENU_ITEM_ICON[4], MENU_ITEM_FW[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_FW[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);

        display->fillRoundRect(MENU_ITEM_UP[0], MENU_ITEM_UP[1], MENU_ITEM_UP[2], MENU_ITEM_UP[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
        display->drawRoundRect(MENU_ITEM_UP[0], MENU_ITEM_UP[1], MENU_ITEM_UP[2], MENU_ITEM_UP[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
        drawIcon(MENU_ITEM_ICON[5], MENU_ITEM_UP[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_UP[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);

        display->fillRoundRect(MENU_ITEM_DOWN[0], MENU_ITEM_DOWN[1], MENU_ITEM_DOWN[2], MENU_ITEM_DOWN[3], MENU_ITEM_RADIUS, BLACK); // Arguments are: start X, start Y, size X, size Y, radius, color
        display->drawRoundRect(MENU_ITEM_DOWN[0], MENU_ITEM_DOWN[1], MENU_ITEM_DOWN[2], MENU_ITEM_DOWN[3], MENU_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
        drawIcon(MENU_ITEM_ICON[6], MENU_ITEM_DOWN[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, MENU_ITEM_DOWN[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2, true);
    }
    else if (currentScreen == MAIN_SCREEN)
    {
        bool showFinishedBooks = libraryManager.getShowFinishedBooks();
        if (showFinishedBooks)
        {
            display->fillRoundRect(MENU_ITEM_SHOW_FINISHED[0], MENU_ITEM_SHOW_FINISHED[1], MENU_ITEM_SHOW_FINISHED[2], MENU_ITEM_SHOW_FINISHED[3], MENU_ITEM_RADIUS, WHITE);
            display->drawRoundRect(MENU_ITEM_SHOW_FINISHED[0], MENU_ITEM_SHOW_FINISHED[1], MENU_ITEM_SHOW_FINISHED[2], MENU_ITEM_SHOW_FINISHED[3], MENU_ITEM_RADIUS, BLACK);
        }
        else
        {
            display->fillRoundRect(MENU_ITEM_SHOW_FINISHED[0], MENU_ITEM_SHOW_FINISHED[1], MENU_ITEM_SHOW_FINISHED[2], MENU_ITEM_SHOW_FINISHED[3], MENU_ITEM_RADIUS, BLACK);
            display->drawRoundRect(MENU_ITEM_SHOW_FINISHED[0], MENU_ITEM_SHOW_FINISHED[1], MENU_ITEM_SHOW_FINISHED[2], MENU_ITEM_SHOW_FINISHED[3], MENU_ITEM_RADIUS, WHITE);
        }

        drawIcon(MENU_ITEM_SHOW_FINISHED_ICON,
                 MENU_ITEM_SHOW_FINISHED[0] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2,
                 MENU_ITEM_SHOW_FINISHED[1] + (MENU_ITEM_SIZE - MENU_ICON_SIZE) / 2,
                 !showFinishedBooks);
    }
    if (partial_update)
    {
        //   Serial.println("Partial Update");
        if (PARTIAL_UPDATE_ALLOWED)
            display->partialUpdate();
        else
            display->display();
    }
}

void UIManager::drawBacklightIndicator()
{
    if (settingsManager.getBacklight() > 0)
    {
        display->fillRoundRect(MENU_ITEM_BACKLIGHT[0] + 5, MENU_ITEM_BACKLIGHT[1] + MENU_ITEM_BACKLIGHT[3] - 3 - int((MENU_ITEM_BACKLIGHT[3] - 5) * settingsManager.getBacklight() / BACKLIGHT_L5), MENU_ITEM_BACKLIGHT[2] / 8, int((MENU_ITEM_BACKLIGHT[3] - 5) * settingsManager.getBacklight() / BACKLIGHT_L5), MENU_ITEM_RADIUS, WHITE);
    }
    else
    {
        display->fillRoundRect(MENU_ITEM_BACKLIGHT[0] + 5, MENU_ITEM_BACKLIGHT[1] + 3, MENU_ITEM_BACKLIGHT[2] / 8, MENU_ITEM_BACKLIGHT[3] - 6, MENU_ITEM_RADIUS, BLACK);
    }
}

void UIManager::renderReadingHeader(bool partial_update)
{
    display->fillRect(HEADER[0], HEADER[1], HEADER[2], HEADER[3], BLACK);

    setFont(FONT_ALT, FONT_SIZE_SMALL);
    display->setTextColor(WHITE);
    display->setCursor(15, 20);
    String bookTitle = libraryManager.getTitle();
    if (bookTitle.length() > LIST_ITEM_MAX_CHARS)
    {
        bookTitle = bookTitle.substring(0, LIST_ITEM_MAX_CHARS - 3) + "..."; // Truncate and add '...'
    }
    display->print(bookTitle);

    display->setCursor(15, display->getCursorY() + NORMAL_LINE_HEIGHT);
    display->print(String(libraryManager.getCurrentPage() + 1) + "." + String(currentSection) + "/" + String(libraryManager.getTotalPage()) + "." + String(totalSections - 1));
    drawBattery(true);

    display->drawRoundRect(READING_HEADER_ITEM_RELOAD[0], READING_HEADER_ITEM_RELOAD[1], READING_HEADER_ITEM_RELOAD[2], READING_HEADER_ITEM_RELOAD[3], HEADER_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
    drawIcon(READING_HEADER_ITEM_ICON[0], READING_HEADER_ITEM_RELOAD[0] + (HEADER_ITEM_SIZE - HEADER_ICON_SIZE) / 2, READING_HEADER_ITEM_RELOAD[1] + (HEADER_ITEM_SIZE - HEADER_ICON_SIZE) / 2, true);

    if (partial_update)
    {
        //   Serial.println("Partial Update");
        if (PARTIAL_UPDATE_ALLOWED)
            display->partialUpdate();
        else
            display->display();
    }
}

void UIManager::renderMainHeader(bool partial_update)
{
    display->fillRect(HEADER[0], HEADER[1], HEADER[2], HEADER[3], BLACK);
    setFont(FONT_ALT, FONT_SIZE_LARGE);
    display->setTextColor(WHITE);
    display->setCursor(15, 45);
    display->print("The Best eBook Reader!");
    drawBattery(true);

    display->drawRoundRect(MAIN_HEADER_ITEM_SETTINGS[0], MAIN_HEADER_ITEM_SETTINGS[1], MAIN_HEADER_ITEM_SETTINGS[2], MAIN_HEADER_ITEM_SETTINGS[3], HEADER_ITEM_RADIUS, WHITE); // Arguments are: start X, start Y, size X, size Y, radius, color
    drawIcon(MAIN_HEADER_ITEM_ICON[0], MAIN_HEADER_ITEM_SETTINGS[0] + (HEADER_ITEM_SIZE - HEADER_ICON_SIZE) / 2, MAIN_HEADER_ITEM_SETTINGS[1] + (HEADER_ITEM_SIZE - HEADER_ICON_SIZE) / 2, true);

    if (partial_update)
    {
        //   Serial.println("Partial Update");
        if (PARTIAL_UPDATE_ALLOWED)
            display->partialUpdate();
        else
            display->display();
    }
}

void UIManager::renderBookList(bool partial_update)
{
    std::vector<BookInfo> bookList = libraryManager.getLibrary();

    setFont(FONT_PRIM, FONT_SIZE_LARGE);
    display->setTextColor(settingsManager.getFgColor());
    display->setCursor(LIST_TITLE_X, LIST_TITLE_Y);
    display->print("MY LIBRARY:");
    int maxScroll = max(0, (int)bookList.size() - LIST_MAX_FILES);

    // Printing book list
    for (int i = 0; i < LIST_MAX_FILES && (scrollIndex + i) < bookList.size(); i++)
    {
        int y = LIST_START_Y + i * LIST_ITEM_HEIGHT;
        display->drawRect(LIST_START_X, y, LIST_WIDTH, LIST_ITEM_HEIGHT, settingsManager.getFgColor());

        // Get the file name and truncate if necessary
        const BookInfo &book = bookList[scrollIndex + i];
        String itemTitle = book.name;
        String itemProgress = String(book.lastPage + 1) + "/" + book.pages;
        if (itemTitle.length() > LIST_ITEM_MAX_CHARS)
        {
            itemTitle = itemTitle.substring(0, LIST_ITEM_MAX_CHARS - 3) + "..."; // Truncate and add '...'
        }
        // renderImage("/library/" + book.name + "/" + book.coverPath, LIST_START_X, y, TOT_W, LIST_ITEM_HEIGHT, ALIGN_LEFT);
        display->setCursor(LIST_START_X + 10, y + 2 * LIST_ITEM_HEIGHT / 5);
        setFont(FONT_PRIM, FONT_SIZE_LARGE);
        display->print(itemTitle);
        setFont(FONT_PRIM, FONT_SIZE_DEFAULT);
        display->setCursor(LIST_START_X + 10, y + 4 * LIST_ITEM_HEIGHT / 5);
        display->print(book.author);
        if (book.isFinished)
        {
            drawIcon(LIST_ICON_FINISHED, LIST_ICON_X, y + 4 * LIST_ITEM_HEIGHT / 5 - LIST_ICON_FINISHED_SIZE, settingsManager.getDarkMode());
        }
        setFont(FONT_ALT, FONT_SIZE_DEFAULT);
        display->setCursor(LIST_WIDTH - itemProgress.length() * 11, y + 4 * LIST_ITEM_HEIGHT / 5);
        display->print(itemProgress);
    }

    // Drawing scrollbar
    if (bookList.size() > LIST_MAX_FILES)
    {
        int scrollbarAreaHeight = (TOT_H - LIST_START_Y - MENU_ITEM_SIZE - 2 * LIST_ARROW_SIZE); // Reserve space for arrows
        int scrollbarHeight = scrollbarAreaHeight * LIST_MAX_FILES / bookList.size();
        int scrollbarY = LIST_START_Y + (scrollbarAreaHeight - scrollbarHeight) * scrollIndex / maxScroll;
        display->fillRect(TOT_W - 2 * LIST_SCROLLBAR_WIDTH - 4, scrollbarY, LIST_SCROLLBAR_WIDTH, scrollbarHeight, settingsManager.getFgColor());

        // Up arrow
        display->fillTriangle(LIST_UP_ARROW_DRAW[0][0], LIST_UP_ARROW_DRAW[0][1],
                              LIST_UP_ARROW_DRAW[1][0], LIST_UP_ARROW_DRAW[1][1],
                              LIST_UP_ARROW_DRAW[2][0], LIST_UP_ARROW_DRAW[2][1], settingsManager.getFgColor());
        display->drawRoundRect(LIST_UP_ARROW_TOUCH[0], LIST_UP_ARROW_TOUCH[1], LIST_UP_ARROW_TOUCH[2], LIST_UP_ARROW_TOUCH[3], LIST_ARROW_RADIUS, settingsManager.getFgColor());
        // Down arrow
        display->fillTriangle(LIST_DOWN_ARROW_DRAW[0][0], LIST_DOWN_ARROW_DRAW[0][1],
                              LIST_DOWN_ARROW_DRAW[1][0], LIST_DOWN_ARROW_DRAW[1][1],
                              LIST_DOWN_ARROW_DRAW[2][0], LIST_DOWN_ARROW_DRAW[2][1], settingsManager.getFgColor());
        display->drawRoundRect(LIST_DOWN_ARROW_TOUCH[0], LIST_DOWN_ARROW_TOUCH[1], LIST_DOWN_ARROW_TOUCH[2], LIST_DOWN_ARROW_TOUCH[3], LIST_ARROW_RADIUS, settingsManager.getFgColor());
    }

    if (partial_update)
    {
        if (PARTIAL_UPDATE_ALLOWED)
            display->partialUpdate();
        else
            display->display();
    }
}

void UIManager::renderSettings(bool partial_update)
{

    display->setTextColor(settingsManager.getFgColor());
    setFont(FONT_ALT, FONT_SIZE_DEFAULT);
    display->drawRect(SETTINGS_PAGE_X, SETTINGS_PAGE_Y, SETTINGS_PAGE_W, SETTINGS_PAGE_H, settingsManager.getFgColor());

    if (currentSubScreen == SETTINGS_TAB_1_ID)
    {
        display->fillRect(SETTINGS_TAB_1[0], SETTINGS_TAB_1[1], SETTINGS_TAB_1[2], SETTINGS_TAB_1[3], settingsManager.getFgColor());
    }
    else
    {
        display->drawRect(SETTINGS_TAB_1[0], SETTINGS_TAB_1[1], SETTINGS_TAB_1[2], SETTINGS_TAB_1[3], settingsManager.getFgColor());
    }

    if (currentSubScreen == SETTINGS_TAB_2_ID)
    {
        display->fillRect(SETTINGS_TAB_2[0], SETTINGS_TAB_2[1], SETTINGS_TAB_2[2], SETTINGS_TAB_2[3], settingsManager.getFgColor());
    }
    else
    {
        display->drawRect(SETTINGS_TAB_2[0], SETTINGS_TAB_2[1], SETTINGS_TAB_2[2], SETTINGS_TAB_2[3], settingsManager.getFgColor());
    }

    if (currentSubScreen == SETTINGS_TAB_3_ID)
    {
        display->fillRect(SETTINGS_TAB_3[0], SETTINGS_TAB_3[1], SETTINGS_TAB_3[2], SETTINGS_TAB_3[3], settingsManager.getFgColor());
    }
    else
    {
        display->drawRect(SETTINGS_TAB_3[0], SETTINGS_TAB_3[1], SETTINGS_TAB_3[2], SETTINGS_TAB_3[3], settingsManager.getFgColor());
    }

    drawIcon(SETTINGS_TAB_ICON[1], SETTINGS_TAB_2[0] + (SETTINGS_TAB_SIZE - SETTINGS_TAB_ICON_SIZE) / 2, SETTINGS_TAB_2[1] + (SETTINGS_TAB_SIZE - SETTINGS_TAB_ICON_SIZE) / 2, (currentSubScreen == SETTINGS_TAB_2_ID) != settingsManager.getDarkMode());
    drawIcon(SETTINGS_TAB_ICON[0], SETTINGS_TAB_1[0] + (SETTINGS_TAB_SIZE - SETTINGS_TAB_ICON_SIZE) / 2, SETTINGS_TAB_1[1] + (SETTINGS_TAB_SIZE - SETTINGS_TAB_ICON_SIZE) / 2, (currentSubScreen == SETTINGS_TAB_1_ID) != settingsManager.getDarkMode());
    drawIcon(SETTINGS_TAB_ICON[2], SETTINGS_TAB_3[0] + (SETTINGS_TAB_SIZE - SETTINGS_TAB_ICON_SIZE) / 2, SETTINGS_TAB_3[1] + (SETTINGS_TAB_SIZE - SETTINGS_TAB_ICON_SIZE) / 2, (currentSubScreen == SETTINGS_TAB_3_ID) != settingsManager.getDarkMode());

    if (currentSubScreen == SETTINGS_TAB_1_ID)
    {
        display->setCursor(SETTINGS_ITEM_1[0], SETTINGS_ITEM_1[1]);
        display->print("Backlight");
        display->setCursor(SETTINGS_PAGE_X + SETTINGS_PAGE_W - 50, SETTINGS_ITEM_1[1]);
        display->print(settingsManager.getBacklight());
        display->drawThickLine(SETTINGS_SEP_1[0], SETTINGS_SEP_1[1], SETTINGS_SEP_1[2], SETTINGS_SEP_1[3], settingsManager.getFgColor(), SETTINGS_SEP_1[4]);

        display->setCursor(SETTINGS_ITEM_2[0], SETTINGS_ITEM_2[1]);
        display->print("Gestures");
        if (settingsManager.getGestures())
        {
            drawIcon(SETTINGS_ITEM_STATUS_ICON[1], SETTINGS_PAGE_X + SETTINGS_PAGE_W - SETTINGS_VALUE_Y_OFFSET, SETTINGS_ITEM_2[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2, settingsManager.getDarkMode());
        }
        else
        {
            drawIcon(SETTINGS_ITEM_STATUS_ICON[0], SETTINGS_PAGE_X + SETTINGS_PAGE_W - SETTINGS_VALUE_Y_OFFSET, SETTINGS_ITEM_2[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2, settingsManager.getDarkMode());
        }
        display->drawThickLine(SETTINGS_SEP_2[0], SETTINGS_SEP_2[1], SETTINGS_SEP_2[2], SETTINGS_SEP_2[3], settingsManager.getFgColor(), SETTINGS_SEP_2[4]);

        display->setCursor(SETTINGS_ITEM_3[0], SETTINGS_ITEM_3[1]);
        display->print("Dark Mode");
        if (settingsManager.getDarkMode())
        {
            drawIcon(SETTINGS_ITEM_STATUS_ICON[1], SETTINGS_PAGE_X + SETTINGS_PAGE_W - SETTINGS_VALUE_Y_OFFSET, SETTINGS_ITEM_3[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2, settingsManager.getDarkMode());
        }
        else
        {
            drawIcon(SETTINGS_ITEM_STATUS_ICON[0], SETTINGS_PAGE_X + SETTINGS_PAGE_W - SETTINGS_VALUE_Y_OFFSET, SETTINGS_ITEM_3[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2, settingsManager.getDarkMode());
        }
    }
    else if (currentSubScreen == SETTINGS_TAB_3_ID)
    {
        display->setCursor(SETTINGS_ITEM_1[0], SETTINGS_ITEM_1[1]);
        display->print("Webserver");
        if (settingsManager.getWebserver())
        {
            drawIcon(SETTINGS_ITEM_STATUS_ICON[1], SETTINGS_PAGE_X + SETTINGS_PAGE_W - SETTINGS_VALUE_Y_OFFSET, SETTINGS_ITEM_1[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2, settingsManager.getDarkMode());
            // Show connection info below the toggle
            setFont(FONT_PRIM, FONT_SIZE_SMALL);
            display->setCursor(SETTINGS_ITEM_1[0], SETTINGS_ITEM_1[1] + FONT_SIZE_DEFAULT_PX + SETTINGS_ITEM_SUBTITLE_Y);
            display->print("SSID: FlickBook  IP: " + webServerManager.getAPIP());
            setFont(FONT_PRIM, FONT_SIZE_DEFAULT);

            // Draw WiFi QR code below connection info
            drawQRCode(webServerManager.getWifiQRString(), QR_WIFI_POS[0], QR_WIFI_POS[1], QR_SIZE, settingsManager.getFgColor(), settingsManager.getBgColor());
        }
        else
        {
            drawIcon(SETTINGS_ITEM_STATUS_ICON[0], SETTINGS_PAGE_X + SETTINGS_PAGE_W - SETTINGS_VALUE_Y_OFFSET, SETTINGS_ITEM_1[1] - SETTINGS_ITEM_STATUS_ICON_SIZE / 2 - FONT_SIZE_DEFAULT_PX / 2, settingsManager.getDarkMode());
        }
    }

    if (partial_update)
    {
        //   Serial.println("Partial Update");
        if (PARTIAL_UPDATE_ALLOWED)
            display->partialUpdate();
        else
            display->display();
    }
}

void UIManager::drawBattery(bool invert)
{
    setFont(FONT_ALT, FONT_SIZE_SMALL);
    display->setTextColor(WHITE);
    float battery_perc = settingsManager.getBatteryPerc();
    drawIcon(ICON_BATTERY, TOT_W - ICON_BATTERY_W, 10, invert);
    display->setCursor(TOT_W - 63, 35);
    display->print(battery_perc, 0);
}

void UIManager::renderLoadingMsg(const String &message, bool partial_update)
{

    clearDisplay();

    // Draw background box
    display->fillRoundRect(LOADING_MSG_X, LOADING_MSG_Y, LOADING_MSG_W, LOADING_MSG_H, 10, settingsManager.getBgColor());
    display->drawRoundRect(LOADING_MSG_X, LOADING_MSG_Y, LOADING_MSG_W, LOADING_MSG_H, 10, settingsManager.getFgColor());

    // Draw loading text
    setFont(FONT_ALT, FONT_SIZE_LARGE);
    display->setTextColor(settingsManager.getFgColor());
    int textX = LOADING_MSG_X + (LOADING_MSG_W - message.length() * 14) / 2;
    int textY = LOADING_MSG_Y + LOADING_MSG_H / 1.5;
    display->setCursor(textX, textY);
    display->print(message);

    // Optionally a simple spinner animation (3 dots)
    // if(PARTIAL_UPDATE_ALLOWED) {
    //   for (int i = 0; i < 3; i++) {
    //       display->fillCircle(boxX + boxW - 30 + i * 8, boxY + boxH/2, 2, BLACK);
    //       delay(150);
    //       display->partialUpdate();
    //   }
    // }

    if (partial_update)
    {
        if (PARTIAL_UPDATE_ALLOWED)
            display->partialUpdate();
        else
            display->display();
    }
    else
    {
        display->display();
    }
}

void UIManager::renderLoadingIcon(bool partial_update)
{

    if (partial_update)
    {
        if (PARTIAL_UPDATE_ALLOWED)
        {

            drawIcon(HEADER_ITEM_LOADING_ICON, HEADER_ITEM_LOADING[0] + (HEADER_ITEM_SIZE - HEADER_ICON_SIZE) / 2, HEADER_ITEM_LOADING[1] + (HEADER_ITEM_SIZE - HEADER_ICON_SIZE) / 2, true);
            display->partialUpdate();
        }
        // else
        // display->display();
    }
    // else
    // {
    // display->display();
    // }
}

/**
 * @brief     Get name of the pucture, create path and draw image on the screen.
 *
 * @return    0 if there is an error, 1 if the image is drawn.
 */
bool UIManager::drawIcon(String iconName, int x, int y, bool invert)
{

    SdFile iconFile;
    String iconsFolder = ICONS_FOLDER;
    String iconFileName = iconName;
    if (iconFileName.lastIndexOf(".") == -1)
    {
        iconFileName = iconFileName + ".png";
    }

    String iconPath = iconsFolder + iconFileName;

    if (!iconFile.open(iconPath.c_str(), O_RDONLY))
    {
        // Serial.println(iconName_str);
        Serial.println("Failed to load icon");
    }

    // Draw the image on the screen
    if (!display->drawImage(iconPath.c_str(), x, y, 1, invert))
    {
        // Close folder and file
        iconFile.close();

        // Return 0 to signalize an error
        return false;
    }

    return 1;
}

void UIManager::renderImage(const String &imgPath, int x, int y, int max_x, int max_y, uint8_t align, bool invert)
{
    Serial.printf("Rendering image at %d, %d: %s\n", x, y, imgPath.c_str());

    if (!imageScaler.drawImageFitTo(imgPath.c_str(), x, y, max_x, max_y, align, true, invert))
    {
        Serial.println("Failed to render image!");
    }
}

void UIManager::drawQRCode(const String &data, int x, int y, int moduleSize, int fgColor, int bgColor)
{
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, data.c_str());

    int qrSize = qrcode.size * moduleSize;
    int padding = moduleSize * 2;
    display->fillRect(x - padding, y - padding, qrSize + 2 * padding, qrSize + 2 * padding, bgColor);

    for (uint8_t row = 0; row < qrcode.size; row++)
    {
        for (uint8_t col = 0; col < qrcode.size; col++)
        {
            if (qrcode_getModule(&qrcode, col, row))
            {
                display->fillRect(x + col * moduleSize, y + row * moduleSize, moduleSize, moduleSize, fgColor);
            }
        }
    }
}