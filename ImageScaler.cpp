/**
 * @file    ImageScaler.cpp
 * @brief   JPEG image scaling implementation using TJpgDec.
 *
 * Bypasses the Inkplate SDK's hardcoded setJpgScale(1) by calling
 * TJpgDec directly with a custom callback that replicates the SDK's
 * Floyd-Steinberg dithering and grayscale conversion.
 */
#include "ImageScaler.h"

extern SDHandler sdHandler;

// Static member definitions
Inkplate *ImageScaler::s_display = nullptr;
int16_t ImageScaler::s_lastY = -1;
int16_t ImageScaler::s_blockW = -1;
int16_t ImageScaler::s_blockH = -1;
uint8_t ImageScaler::s_ditherBuffer[2][E_INK_WIDTH + 20] = {};
uint8_t ImageScaler::s_jpegDitherBuffer[18][18] = {};

ImageScaler::ImageScaler(Inkplate *display)
    : _display(display) {}

bool ImageScaler::isJpeg(const char *path)
{
    String p = String(path);
    p.toLowerCase();
    return p.endsWith(".jpg") || p.endsWith(".jpeg");
}

/**
 * Pick the smallest TJpgDec scale factor (largest image) that fits within maxW x maxH.
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
        Serial.println("[ImageScaler] ps_malloc failed");
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
    s_lastY = -1;
    s_blockW = -1;
    s_blockH = -1;
    memset(s_ditherBuffer, 0, sizeof(s_ditherBuffer));
    memset(s_jpegDitherBuffer, 0, sizeof(s_jpegDitherBuffer));

    // Configure TJpgDec with our scale and callback
    TJpgDec.setJpgScale(scaleFactor);
    TJpgDec.setCallback(jpegChunkCallback);

    JRESULT result = TJpgDec.drawJpg(x, y, buff, total, dither, invert);
    free(buff);

    if (result != JDR_OK)
        Serial.printf("[ImageScaler] drawJpg error: %d\n", result);

    return result == JDR_OK;
}

/**
 * TJpgDec decode callback — replicates the Inkplate SDK's drawJpegChunk
 * for non-color boards with identical Floyd-Steinberg dithering.
 * Uses Adafruit_GFX public interface for batched pixel writes.
 */
bool ImageScaler::jpegChunkCallback(int16_t x, int16_t y, uint16_t w, uint16_t h,
                                    uint16_t *bitmap, bool dither, bool invert)
{
    if (!s_display)
        return false;

    // Dither row transition: propagate error from previous row of blocks
    if (dither && y != s_lastY)
    {
        for (int i = 0; i < E_INK_WIDTH + 20; ++i)
        {
            s_ditherBuffer[0][i] = s_ditherBuffer[1][i];
            s_ditherBuffer[1][i] = 0;
        }
        s_lastY = y;
    }

    // Use Adafruit_GFX public interface for batched writes
    Adafruit_GFX *gfx = static_cast<Adafruit_GFX *>(s_display);
    gfx->startWrite();

    bool is1Bit = (s_display->getDisplayMode() == INKPLATE_1BIT);

    for (int j = 0; j < h; ++j)
    {
        for (int i = 0; i < w; ++i)
        {
            uint32_t rgb = bitmap[j * w + i];
            uint8_t r = _RED(rgb);
            uint8_t g = _GREEN(rgb);
            uint8_t b = _BLUE(rgb);

            uint32_t val;

            if (dither)
            {
                // Floyd-Steinberg dithering (matches SDK's ditherGetPixelJpeg)
                uint8_t px = RGB8BIT(r, g, b);

                if (s_blockW == -1)
                {
                    s_blockW = w;
                    s_blockH = h;
                }

                if (is1Bit)
                    px = (uint16_t)px >> 1;

                uint16_t oldPixel = min((uint16_t)0xFF,
                                        (uint16_t)((uint16_t)px +
                                                   (uint16_t)s_jpegDitherBuffer[j + 1][i + 1] +
                                                   (j ? (uint16_t)0 : (uint16_t)s_ditherBuffer[0][x + i])));

                uint8_t newPixel = oldPixel & (is1Bit ? B10000000 : B11100000);
                uint8_t quantError = oldPixel - newPixel;

                s_jpegDitherBuffer[j + 1 + 1][i + 0 + 1] += (quantError * 5) >> 4;
                s_jpegDitherBuffer[j + 0 + 1][i + 1 + 1] += (quantError * 7) >> 4;
                s_jpegDitherBuffer[j + 1 + 1][i + 1 + 1] += (quantError * 1) >> 4;
                s_jpegDitherBuffer[j + 1 + 1][i - 1 + 1] += (quantError * 3) >> 4;

                val = newPixel >> 5;
            }
            else
            {
                val = RGB3BIT(r, g, b);
            }

            if (is1Bit)
            {
                val = (~val >> 2) & 1;
                if (invert)
                    val = 1 - val;
            }
            else
            {
                if (invert)
                    val = 7 - val;
            }

            gfx->writePixel(x + i, y + j, val);
        }
    }

    // Propagate dithering errors between horizontal blocks
    if (dither)
    {
        for (int i = 0; i < 18; ++i)
        {
            if (x + i)
                s_ditherBuffer[1][x + i - 1] += s_jpegDitherBuffer[s_blockH - 1 + 2][i];
            s_jpegDitherBuffer[i][0 + 1] = s_jpegDitherBuffer[i][s_blockW - 1 + 2];
        }
        for (int j = 0; j < 18; ++j)
            for (int i = 0; i < 18; ++i)
                if (i != 1)
                    s_jpegDitherBuffer[j][i] = 0;
        s_jpegDitherBuffer[17][1] = 0;
    }

    gfx->endWrite();

    return true;
}
