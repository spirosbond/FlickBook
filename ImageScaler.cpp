/**
 * @file    ImageScaler.cpp
 * @brief   JPEG image scaling implementation using JPEGDEC.
 *
 * Uses bitbank2/JPEGDEC for JPEG decoding with built-in Floyd-Steinberg
 * dithering and support for progressive JPEG.
 */
#include "ImageScaler.h"

// Arduino only auto-compiles .cpp/.c files in the sketch root and src/ — not
// arbitrary subfolders like libs/. Following the project convention (see
// UIManagerRendering.cpp including libs/qrcode.c), we pull the JPEGDEC
// implementation into this translation unit directly. ImageScaler is its sole
// consumer, so this produces no duplicate symbols.
#include "libs/JPEGDEC.cpp"

extern SDHandler sdHandler;

// Static member definitions
Inkplate *ImageScaler::s_display = nullptr;
bool ImageScaler::s_dither = false;
bool ImageScaler::s_invert = false;

ImageScaler::ImageScaler(Inkplate *display)
    : _display(display) {}

bool ImageScaler::isJpeg(const char *path)
{
    String p = String(path);
    p.toLowerCase();
    return p.endsWith(".jpg") || p.endsWith(".jpeg");
}

/**
 * Pick the smallest scale factor (largest image) that fits within maxW x maxH.
 * Returns 1, 2, 4, or 8.
 */
uint8_t ImageScaler::pickBestScaleFactor(int origW, int origH, int maxW, int maxH)
{
    const uint8_t scales[] = {1, 2, 4, 8};
    for (int i = 0; i < 4; i++)
    {
        int sw = (origW + scales[i] - 1) / scales[i];
        int sh = (origH + scales[i] - 1) / scales[i];
        if (sw <= maxW && sh <= maxH)
            return scales[i];
    }
    return 8;
}

bool ImageScaler::getScaledDimensions(const char *path, int maxW, int maxH,
                                      int &outW, int &outH)
{
    int origW, origH;
    if (!sdHandler.getImageDimensions(String(path), origW, origH))
        return false;

    if (isJpeg(path) && (origW > maxW || origH > maxH))
    {
        uint8_t scale = pickBestScaleFactor(origW, origH, maxW, maxH);
        outW = (origW + scale - 1) / scale;
        outH = (origH + scale - 1) / scale;
    }
    else
    {
        outW = origW;
        outH = origH;
    }
    return true;
}

bool ImageScaler::drawImageFitTo(const char *path, int x, int y, int maxW, int maxH, uint8_t align,
                                 bool dither, bool invert)
{
    int origW, origH;
    if (!sdHandler.getImageDimensions(String(path), origW, origH))
        return false;

    if (isJpeg(path) && (origW > maxW || origH > maxH))
    {
        uint8_t scale = pickBestScaleFactor(origW, origH, maxW, maxH);
        int scaledW = (origW + scale - 1) / scale;
        int offsetX = 0;
        switch (align)
        {
        case ALIGN_LEFT:
            // Center horizontally within maxW
            offsetX = 0;
            break;

        case ALIGN_CENTER:
            // Center horizontally within maxW
            offsetX = (maxW - scaledW) / 2;
            break;

        case ALIGN_RIGHT:
            // Center horizontally within maxW
            offsetX = (maxW - scaledW);
            break;

        default:
            // Center horizontally within maxW
            offsetX = (maxW - scaledW) / 2;
            break;
        }

        Serial.printf("[ImageScaler] JPEG %dx%d -> 1/%d = %dx%d\n",
                      origW, origH, scale, scaledW, (origH + scale - 1) / scale);

        return drawJpegWithScale(path, x + offsetX, y, scale, dither, invert);
    }
    else
    {
        // Image fits or is not JPEG: draw at original size, centered
        if (origW < maxW)
        {
            int offsetX = (maxW - origW) / 2;
            x += offsetX;
        }
        return _display->drawImage(path, x, y, dither, invert);
    }
}

bool ImageScaler::drawJpegWithScale(const char *path, int x, int y, uint8_t scaleFactor,
                                    bool dither, bool invert)
{
    SdFile dat;
    if (!dat.open(path, O_RDONLY))
    {
        Serial.printf("[ImageScaler] Failed to open: %s\n", path);
        return false;
    }

    uint32_t total = dat.fileSize();
    uint8_t *buff = (uint8_t *)ps_malloc(total);
    if (!buff)
    {
        Serial.println("[ImageScaler] ps_malloc failed for file buffer");
        dat.close();
        return false;
    }

    // Read entire file into PSRAM buffer
    uint32_t pnt = 0;
    while (pnt < total)
    {
        uint32_t toread = dat.available();
        if (toread > 0)
        {
            int bytesRead = dat.read(buff + pnt, toread);
            if (bytesRead > 0)
                pnt += bytesRead;
        }
    }
    dat.close();

    // Initialize static state for the callback
    s_display = _display;
    s_dither = dither;
    s_invert = invert;

    // Map scale factor to JPEGDEC decode options
    int decodeOptions = 0;
    switch (scaleFactor)
    {
    case 2:
        decodeOptions = JPEG_SCALE_HALF;
        break;
    case 4:
        decodeOptions = JPEG_SCALE_QUARTER;
        break;
    case 8:
        decodeOptions = JPEG_SCALE_EIGHTH;
        break;
    default:
        break;
    }

    // Heap-allocate JPEGDEC (~17.5KB internal state — too large for ESP32 stack)
    JPEGDEC *jpeg = new JPEGDEC();
    if (!jpeg)
    {
        Serial.println("[ImageScaler] Failed to allocate JPEGDEC");
        free(buff);
        return false;
    }

    if (!jpeg->openRAM(buff, total, jpegDrawCallback))
    {
        Serial.printf("[ImageScaler] JPEGDEC openRAM failed, error: %d\n", jpeg->getLastError());
        delete jpeg;
        free(buff);
        return false;
    }

    Serial.printf("[ImageScaler] JPEG %dx%d, type: %s, subsample: 0x%02X\n",
                  jpeg->getWidth(), jpeg->getHeight(),
                  jpeg->getJPEGType() == JPEG_MODE_PROGRESSIVE ? "Progressive" : "Baseline",
                  jpeg->getSubSample());

    if (jpeg->getJPEGType() == JPEG_MODE_PROGRESSIVE)
        Serial.println("[ImageScaler] Progressive JPEG: decode may produce DC-only (thumbnail) quality");

    bool is1Bit = (_display->getDisplayMode() == INKPLATE_1BIT);
    int result;

    if (dither)
    {
        jpeg->setPixelType(is1Bit ? ONE_BIT_DITHERED : FOUR_BIT_DITHERED);

        // Pad width to MCU boundary (16px) for safe dither buffer sizing
        int ditherW = (jpeg->getWidth() + 15) & ~15;
        uint8_t *ditherBuf = (uint8_t *)ps_malloc(ditherW * 16);
        if (!ditherBuf)
        {
            Serial.println("[ImageScaler] ps_malloc failed for dither buffer");
            jpeg->close();
            delete jpeg;
            free(buff);
            return false;
        }

        result = jpeg->decodeDither(x, y, ditherBuf, decodeOptions);
        free(ditherBuf);
    }
    else
    {
        jpeg->setPixelType(EIGHT_BIT_GRAYSCALE);
        result = jpeg->decode(x, y, decodeOptions);
    }

    if (!result)
        Serial.printf("[ImageScaler] JPEGDEC decode failed, error: %d\n", jpeg->getLastError());

    jpeg->close();
    delete jpeg;
    free(buff);

    return result == 1;
}

/**
 * JPEGDEC draw callback — renders decoded pixel blocks to the Inkplate display.
 * Handles three pixel formats based on the decode mode:
 *   - ONE_BIT_DITHERED (iBpp=1): packed bits with built-in F-S dithering
 *   - FOUR_BIT_DITHERED (iBpp=4): packed nibbles, mapped to 3-bit for Inkplate
 *   - EIGHT_BIT_GRAYSCALE (iBpp=8): direct grayscale, quantized to display depth
 */
int ImageScaler::jpegDrawCallback(JPEGDRAW *pDraw)
{
    if (!s_display)
        return 0;

    bool is1Bit = (s_display->getDisplayMode() == INKPLATE_1BIT);
    Adafruit_GFX *gfx = static_cast<Adafruit_GFX *>(s_display);
    gfx->startWrite();

    uint8_t *pixels = (uint8_t *)pDraw->pPixels;
    // Use iWidthUsed for edge blocks; iWidth for pixel array stride/pitch
    int drawW = pDraw->iWidthUsed;

    for (int j = 0; j < pDraw->iHeight; j++)
    {
        for (int i = 0; i < drawW; i++)
        {
            uint8_t val;

            if (pDraw->iBpp == 1)
            {
                // ONE_BIT_DITHERED: packed bits, MSB first, per-row byte alignment
                int pitch = (pDraw->iWidth + 7) >> 3;
                uint8_t bit = (pixels[j * pitch + (i >> 3)] >> (7 - (i & 7))) & 1;
                // JPEGDEC: 0=black, 1=white; Inkplate 1-bit: WHITE=0, BLACK=1
                val = 1 - bit;
                if (s_invert)
                    val = 1 - val;
            }
            else if (pDraw->iBpp == 4)
            {
                // FOUR_BIT_DITHERED: packed nibbles, high nibble first
                int pitch = (pDraw->iWidth + 1) >> 1;
                uint8_t nibble;
                if (i & 1)
                    nibble = pixels[j * pitch + (i >> 1)] & 0x0F;
                else
                    nibble = (pixels[j * pitch + (i >> 1)] >> 4) & 0x0F;

                // Map 0-15 → 0-7 for Inkplate 3-bit grayscale (0=black, 7=white)
                val = nibble >> 1;
                if (s_invert)
                    val = 7 - val;
            }
            else
            {
                // EIGHT_BIT_GRAYSCALE (no-dither path)
                uint8_t gray = pixels[j * pDraw->iWidth + i];
                if (is1Bit)
                {
                    // Inkplate 1-bit: WHITE=0, BLACK=1
                    val = (gray < 128) ? 1 : 0;
                    if (s_invert)
                        val = 1 - val;
                }
                else
                {
                    // Inkplate 3-bit: 0=black, 7=white
                    val = gray >> 5;
                    if (s_invert)
                        val = 7 - val;
                }
            }

            gfx->writePixel(pDraw->x + i, pDraw->y + j, val);
        }
    }

    gfx->endWrite();
    return 1;
}
