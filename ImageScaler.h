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
                        bool dither = true, bool invert = false, bool andCache = false, bool exactFit = false);

    /**
     * Get the dimensions the image would have after scaling to fit maxW x maxH.
     * For JPEG: returns dimensions after best-fit scaling (power-of-two), or, when
     * exactFit is set, the aspect-preserving contain-fit size (matching how grid
     * covers are actually rendered).
     * For PNG/BMP: returns original dimensions (no scaling).
     */
    bool getScaledDimensions(const char *path, int maxW, int maxH,
                             int &outW, int &outH, bool exactFit = false);

    /**
     * Draw a JPEG with a specific scale factor.
     * @param scaleFactor  1 (full), 2 (half), 4 (quarter), or 8 (eighth)
     */
    bool drawJpegWithScale(const char *path, int x, int y, uint8_t scaleFactor,
                           bool dither = true, bool invert = false);

    /**
     * Draw a JPEG scaled to exactly outW x outH (aspect ratio is the caller's
     * responsibility) using the sharp decode -> area-average -> dither pipeline.
     */
    bool drawJpegToSize(const char *path, int x, int y, int outW, int outH,
                        bool dither = true, bool invert = false);

    /**
     * Blit a cached pre-dithered thumbnail (PDT) at (x, y). Returns false if the
     * file is missing/invalid or its bit-depth doesn't match the display.
     */
    bool drawCachedBitmap(const char *cachePath, int x, int y);

    /** Derive the cache path for a source image + scale factor. */
    static String deriveCachePath(const char *srcPath, uint8_t scaleFactor);

    /** Derive the cache path for a source image + explicit output size. */
    static String deriveCachePathBySize(const char *srcPath, int outW, int outH);

private:
    Inkplate *_display;

    static bool isJpeg(const char *path);
    static bool isProgressiveJpeg(const char *path);
    static uint8_t pickBestScaleFactor(int origW, int origH, int maxW, int maxH);

    // No-upscale "contain" fit: the largest outW x outH that fits within
    // maxW x maxH while preserving aspect ratio, never exceeding the original.
    static void computeContainFit(int origW, int origH, int maxW, int maxH,
                                  int &outW, int &outH);

    // Largest power-of-two decode prescale (1/2/4/8) that keeps the decoded
    // buffer >= the target size in both dims (so area-averaging only downscales),
    // bumped further if the decoded buffer would exceed the memory cap.
    static uint8_t chooseDecodeScale(int origW, int origH, int outW, int outH);

    // Decode a JPEG into a freshly allocated PSRAM grayscale buffer at the given
    // decode scale (1/2/4/8). Caller frees. Returns null on failure.
    uint8_t *decodeJpegToGray(const char *path, uint8_t reqScale,
                              int &decW, int &decH, bool &isProgressive);

    // Generate (and write) a single pre-dithered cache file for one scale factor,
    // baked at the current display bit-depth. Used lazily on the first render.
    bool generateScaledCache(const char *srcPath, uint8_t scaleFactor);

    // Generate (and write) a pre-dithered cache file at an explicit output size.
    bool generateExactCache(const char *srcPath, int outW, int outH);

    // Write one cache file at a specific output size from an already-decoded
    // grayscale buffer (area-averaged + dithered to the display bit-depth).
    bool writeScaledCache(const uint8_t *gray, int gw, int gh,
                          int outW, int outH, const char *dstPath);

    // JPEGDEC draw callback. Two modes:
    //  - capture mode (s_captureBuf != null): copies decoded grayscale into a
    //    PSRAM buffer for high-quality software downscaling.
    //  - streaming mode: applies the Inkplate SDK's Floyd-Steinberg dithering
    //    directly (used for full-resolution / scale-1 draws).
    static int jpegDrawCallback(JPEGDRAW *pDraw);

    // Area-average a full-resolution grayscale buffer down to outW x outH, then
    // Floyd-Steinberg dither (matching the Inkplate SDK). If outRaster is null the
    // result is blitted to the display at (x, y); otherwise it is packed into
    // outRaster (1 bpp on 1-bit displays, 4 bpp on 3-bit displays).
    void ditherBlitDownscaled(const uint8_t *gray, int gw, int gh,
                              int outW, int outH, int x, int y,
                              bool dither, bool invert,
                              uint8_t *outRaster = nullptr);

    // Static state for the callback
    static Inkplate *s_display;
    static bool s_dither;
    static bool s_invert;
    static int16_t s_lastY;
    static int16_t s_blockW;
    static int16_t s_blockH;
    static uint8_t s_ditherBuffer[2][E_INK_WIDTH + 20];
    static uint8_t s_jpegDitherBuffer[18][18];

    // Capture-mode framebuffer (grayscale, gw*gh); when non-null the callback
    // copies decoded pixels here instead of drawing to the display.
    static uint8_t *s_captureBuf;
    static int s_captureW;
    static int s_captureH;
};

#endif
