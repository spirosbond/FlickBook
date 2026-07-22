/**
 * @file    ImageScaler.h
 * @brief   JPEG image scaling for Inkplate e-ink displays using JPEGDEC.
 *
 * Provides scaled image drawing without modifying the Inkplate SDK.
 * For JPEG images, leverages JPEGDEC's decode-time scaling (1/2, 1/4, 1/8)
 * with built-in Floyd-Steinberg dithering. Supports baseline and progressive
 * JPEG (progressive decodes at DC-only / thumbnail quality).
 * For PNG/BMP, falls back to the SDK's unscaled drawImage().
 *
 * Requires: bitbank2/JPEGDEC library (install via Arduino Library Manager)
 */
#ifndef IMAGE_SCALER_H
#define IMAGE_SCALER_H

#include "Inkplate.h"
#include "SDHandler.h"
// #include <JPEGDEC.h>
#include "libs/JPEGDEC.h"

#define ALIGN_LEFT (0)
#define ALIGN_CENTER (1)
#define ALIGN_RIGHT (2)

class ImageScaler
{
public:
    ImageScaler(Inkplate *display);

    /**
     * Draw an image scaled to fit within maxW x maxH, maintaining aspect ratio.
     * JPEG images are scaled using JPEGDEC (1/2, 1/4, 1/8 ratios).
     * PNG/BMP images are drawn at original size via the SDK.
     * The image is centered horizontally within maxW.
     */
    bool drawImageFitTo(const char *path, int x, int y, int maxW, int maxH, uint8_t align = ALIGN_CENTER,
                        bool dither = true, bool invert = false);

    /**
     * Get the dimensions the image would have after scaling to fit maxW x maxH.
     * For JPEG: returns dimensions after best-fit scaling.
     * For PNG/BMP: returns original dimensions (no scaling).
     */
    bool getScaledDimensions(const char *path, int maxW, int maxH,
                             int &outW, int &outH);

    /**
     * Draw a JPEG with a specific scale factor.
     * @param scaleFactor  1 (full), 2 (half), 4 (quarter), or 8 (eighth)
     */
    bool drawJpegWithScale(const char *path, int x, int y, uint8_t scaleFactor,
                           bool dither = true, bool invert = false);

private:
    Inkplate *_display;

    static bool isJpeg(const char *path);
    static uint8_t pickBestScaleFactor(int origW, int origH, int maxW, int maxH);

    // JPEGDEC draw callback
    static int jpegDrawCallback(JPEGDRAW *pDraw);

    // Static state for the callback
    static Inkplate *s_display;
    static bool s_dither;
    static bool s_invert;
};

#endif
