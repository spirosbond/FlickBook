/**
 * @file    ImageScaler.h
 * @brief   JPEG image scaling for Inkplate e-ink displays using TJpgDec.
 *
 * Provides scaled image drawing without modifying the Inkplate SDK.
 * For JPEG images, leverages TJpgDec's built-in decode-time scaling
 * (1/2, 1/4, 1/8) with Floyd-Steinberg dithering matching the SDK quality.
 * For PNG/BMP, falls back to the SDK's unscaled drawImage().
 */
#ifndef IMAGE_SCALER_H
#define IMAGE_SCALER_H

#include "Inkplate.h"
#include "SDHandler.h"
#include "libs/TJpg_Decoder.h"

#define ALIGN_LEFT (0)
#define ALIGN_CENTER (1)
#define ALIGN_RIGHT (2)

class ImageScaler
{
public:
    ImageScaler(Inkplate *display);

    /**
     * Draw an image scaled to fit within maxW x maxH, maintaining aspect ratio.
     * JPEG images are scaled using TJpgDec (1/2, 1/4, 1/8 ratios).
     * PNG/BMP images are drawn at original size via the SDK.
     * The image is centered horizontally within maxW.
     */
    bool drawImageFitTo(const char *path, int x, int y, int maxW, int maxH, uint8_t align = ALIGN_CENTER,
                        bool dither = true, bool invert = false);

    /**
     * Get the dimensions the image would have after scaling to fit maxW x maxH.
     * For JPEG: returns dimensions after best-fit TJpgDec scaling.
     * For PNG/BMP: returns original dimensions (no scaling).
     */
    bool getScaledDimensions(const char *path, int maxW, int maxH,
                             int &outW, int &outH);

    /**
     * Draw a JPEG with a specific TJpgDec scale factor.
     * @param scaleFactor  1 (full), 2 (half), 4 (quarter), or 8 (eighth)
     */
    bool drawJpegWithScale(const char *path, int x, int y, uint8_t scaleFactor,
                           bool dither = true, bool invert = false);

private:
    Inkplate *_display;

    static bool isJpeg(const char *path);
    static uint8_t pickBestScaleFactor(int origW, int origH, int maxW, int maxH);

    // TJpgDec decode callback with Floyd-Steinberg dithering
    static bool jpegChunkCallback(int16_t x, int16_t y, uint16_t w, uint16_t h,
                                  uint16_t *bitmap, bool dither, bool invert);

    // Static state for the callback (TJpgDec requires a C function pointer)
    static Inkplate *s_display;
    static int16_t s_lastY;
    static int16_t s_blockW;
    static int16_t s_blockH;
    static uint8_t s_ditherBuffer[2][E_INK_WIDTH + 20];
    static uint8_t s_jpegDitherBuffer[18][18];
};

#endif
