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

// ---------------------------------------------------------------------------
// PDT ("Pre-Dithered Thumbnail") cache format
// 12-byte header, little-endian, followed by packed pixel payload:
//   magic[4] = 'P','D','T','1'
//   uint8  bitDepth   (1 = 1-bit display, 3 = 3-bit display)
//   uint8  reserved   (0)
//   uint16 width, height
// Payload (rows padded to whole bytes):
//   bitDepth 1 -> 1 bpp, MSB-first, value = final display pixel (0/1)
//   bitDepth 3 -> 4 bpp, high nibble first, value = final display pixel (0..7)
// ---------------------------------------------------------------------------
#define PDT_HEADER_SIZE 12
static const uint8_t PDT_MAGIC[4] = {'P', 'D', 'T', '1'};

// Static member definitions
Inkplate *ImageScaler::s_display = nullptr;
bool ImageScaler::s_dither = false;
bool ImageScaler::s_invert = false;
int16_t ImageScaler::s_lastY = -1;
int16_t ImageScaler::s_blockW = -1;
int16_t ImageScaler::s_blockH = -1;
uint8_t ImageScaler::s_ditherBuffer[2][E_INK_WIDTH + 20] = {};
uint8_t ImageScaler::s_jpegDitherBuffer[18][18] = {};
uint8_t *ImageScaler::s_captureBuf = nullptr;
int ImageScaler::s_captureW = 0;
int ImageScaler::s_captureH = 0;

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
                                 bool dither, bool invert, bool andCache)
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

        // Cache path (covers): only when the caller opts in via andCache and for
        // the dither=true, invert=false configuration caches are baked with.
        // Lazily create the single-scale cache on first use, then blit it;
        // subsequent renders skip decoding entirely.
        if (andCache && dither && !invert)
        {
            String cachePath = deriveCachePath(path, scale);
            if (!sdHandler.fileExists(cachePath))
                generateScaledCache(path, scale);
            if (sdHandler.fileExists(cachePath) &&
                drawCachedBitmap(cachePath.c_str(), x + offsetX, y))
                return true;
        }

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
    s_lastY = -1;
    s_blockW = -1;
    s_blockH = -1;
    s_captureBuf = nullptr;
    s_captureW = 0;
    s_captureH = 0;
    memset(s_ditherBuffer, 0, sizeof(s_ditherBuffer));
    memset(s_jpegDitherBuffer, 0, sizeof(s_jpegDitherBuffer));

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

    const int origW = jpeg->getWidth();
    const int origH = jpeg->getHeight();
    const bool isProgressive = (jpeg->getJPEGType() == JPEG_MODE_PROGRESSIVE);

    Serial.printf("[ImageScaler] JPEG %dx%d, type: %s, subsample: 0x%02X\n",
                  origW, origH, isProgressive ? "Progressive" : "Baseline",
                  jpeg->getSubSample());

    if (isProgressive)
        Serial.println("[ImageScaler] Progressive JPEG: decode may produce DC-only (thumbnail) quality");

    jpeg->setPixelType(EIGHT_BIT_GRAYSCALE);
    jpeg->setMaxOutputSize(1); // one MCU per callback (blocks <= 16px wide)

    // JPEGDEC scale flag matching the requested output scale factor. Used for
    // the direct streaming path (full-page, progressive, or low-memory fallback).
    int fitOptions = 0;
    switch (scaleFactor)
    {
    case 2: fitOptions = JPEG_SCALE_HALF; break;
    case 4: fitOptions = JPEG_SCALE_QUARTER; break;
    case 8: fitOptions = JPEG_SCALE_EIGHTH; break;
    default: break;
    }

    int result;

    // Progressive JPEGs decode DC-only (already low detail) and JPEGDEC needs
    // large internal buffers for them, so the full-decode-and-average path would
    // both waste memory and gain nothing — stream them directly instead.
    if (scaleFactor <= 1 || isProgressive)
    {
        // Stream MCUs straight to the display with the Inkplate SDK's block-based
        // Floyd-Steinberg dithering (in the callback).
        result = jpeg->decode(x, y, fitOptions);
    }
    else
    {
        // Downscale: JPEGDEC's built-in 1/2, 1/4, 1/8 modes use a reduced IDCT
        // (blurry). Instead decode at full IDCT into PSRAM and area-average down,
        // matching the sharpness of the previous TJpgDec implementation.
        const int outW = (origW + scaleFactor - 1) / scaleFactor;
        const int outH = (origH + scaleFactor - 1) / scaleFactor;

        // Pick the finest decode scale (max detail) that stays under a memory
        // cap; never finer resolution than needed, never coarser than the target.
        const uint32_t CAP_PIXELS = 1500000; // ~1.5 MB grayscale buffer
        uint8_t decScale = 1;
        int decW = origW, decH = origH;
        while ((uint32_t)decW * decH > CAP_PIXELS && decScale < scaleFactor)
        {
            decScale <<= 1;
            decW = (origW + decScale - 1) / decScale;
            decH = (origH + decScale - 1) / decScale;
        }

        int decOptions = 0;
        switch (decScale)
        {
        case 2: decOptions = JPEG_SCALE_HALF; break;
        case 4: decOptions = JPEG_SCALE_QUARTER; break;
        case 8: decOptions = JPEG_SCALE_EIGHTH; break;
        default: break;
        }

        uint8_t *fb = (uint8_t *)ps_malloc((size_t)decW * decH);
        if (!fb)
        {
            // Not enough PSRAM for the high-quality path — fall back to JPEGDEC's
            // built-in (blurrier) downscale streamed directly to the display.
            Serial.println("[ImageScaler] decode framebuffer alloc failed; using direct downscale");
            result = jpeg->decode(x, y, fitOptions);
        }
        else
        {
            // Capture mode: callback copies decoded grayscale into fb at (0,0).
            s_captureBuf = fb;
            s_captureW = decW;
            s_captureH = decH;
            result = jpeg->decode(0, 0, decOptions);
            s_captureBuf = nullptr;

            if (result)
                ditherBlitDownscaled(fb, decW, decH, outW, outH, x, y, dither, invert);

            free(fb);
        }
    }

    if (!result)
        Serial.printf("[ImageScaler] JPEGDEC decode failed, error: %d\n", jpeg->getLastError());

    jpeg->close();
    delete jpeg;
    free(buff);

    return result == 1;
}

/**
 * Area-average a full-resolution grayscale buffer (gw x gh) down to outW x outH,
 * then apply the Inkplate SDK's Floyd-Steinberg dithering and blit at (x, y).
 * Area averaging preserves detail (anti-aliased), unlike JPEGDEC's reduced-IDCT
 * downscale which is blurry.
 */
void ImageScaler::ditherBlitDownscaled(const uint8_t *gray, int gw, int gh,
                                       int outW, int outH, int x, int y,
                                       bool dither, bool invert,
                                       uint8_t *outRaster)
{
    if (outW <= 0 || outH <= 0)
        return;

    const bool is1Bit = (_display->getDisplayMode() == INKPLATE_1BIT);
    const bool toRaster = (outRaster != nullptr);
    Adafruit_GFX *gfx = static_cast<Adafruit_GFX *>(_display);

    // Packed-raster row stride: 1 bpp (1-bit) or 4 bpp (3-bit)
    const int rasterPitch = is1Bit ? ((outW + 7) >> 3) : ((outW + 1) >> 1);

    // Two running error rows for Floyd-Steinberg (width padded by 1 on each side)
    int32_t *errCur = (int32_t *)ps_malloc((size_t)(outW + 2) * sizeof(int32_t));
    int32_t *errNext = (int32_t *)ps_malloc((size_t)(outW + 2) * sizeof(int32_t));
    if (!errCur || !errNext)
    {
        Serial.println("[ImageScaler] ps_malloc failed for dither error rows");
        if (errCur) free(errCur);
        if (errNext) free(errNext);
        return;
    }
    memset(errCur, 0, (size_t)(outW + 2) * sizeof(int32_t));
    memset(errNext, 0, (size_t)(outW + 2) * sizeof(int32_t));

    if (!toRaster)
        gfx->startWrite();

    for (int oy = 0; oy < outH; ++oy)
    {
        int sy0 = (int)((int64_t)oy * gh / outH);
        int sy1 = (int)((int64_t)(oy + 1) * gh / outH);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sy1 > gh) sy1 = gh;

        for (int ox = 0; ox < outW; ++ox)
        {
            int sx0 = (int)((int64_t)ox * gw / outW);
            int sx1 = (int)((int64_t)(ox + 1) * gw / outW);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sx1 > gw) sx1 = gw;

            // Box-average the source region into a single luminance value
            uint32_t sum = 0, cnt = 0;
            for (int sy = sy0; sy < sy1; ++sy)
            {
                const uint8_t *row = gray + (size_t)sy * gw;
                for (int sx = sx0; sx < sx1; ++sx)
                {
                    sum += row[sx];
                    ++cnt;
                }
            }
            int lum = cnt ? (int)(sum / cnt) : 0;

            uint32_t val;
            if (dither)
            {
                if (is1Bit)
                    lum >>= 1; // SDK darkening bias for 1-bit output

                int oldPixel = lum + errCur[ox + 1];
                if (oldPixel < 0) oldPixel = 0;
                if (oldPixel > 0xFF) oldPixel = 0xFF;

                int newPixel = oldPixel & (is1Bit ? 0x80 : 0xE0);
                int quantError = oldPixel - newPixel;

                // Floyd-Steinberg diffusion: 7/16 right, 3/16 below-left,
                // 5/16 below, 1/16 below-right (matches the SDK weights)
                errCur[ox + 2] += (quantError * 7) >> 4;
                errNext[ox + 0] += (quantError * 3) >> 4;
                errNext[ox + 1] += (quantError * 5) >> 4;
                errNext[ox + 2] += (quantError * 1) >> 4;

                val = newPixel >> 5;
            }
            else
            {
                val = lum >> 5;
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

            if (toRaster)
            {
                // Pack the final display value: 1 bpp (MSB-first) or 4 bpp (high nibble first)
                uint8_t *rrow = outRaster + (size_t)oy * rasterPitch;
                if (is1Bit)
                {
                    if (val & 1)
                        rrow[ox >> 3] |= (uint8_t)(0x80 >> (ox & 7));
                }
                else
                {
                    if (ox & 1)
                        rrow[ox >> 1] |= (uint8_t)(val & 0x0F);
                    else
                        rrow[ox >> 1] |= (uint8_t)((val & 0x0F) << 4);
                }
            }
            else
            {
                gfx->writePixel(x + ox, y + oy, val);
            }
        }

        // Advance error rows
        int32_t *tmp = errCur;
        errCur = errNext;
        errNext = tmp;
        memset(errNext, 0, (size_t)(outW + 2) * sizeof(int32_t));
    }

    if (!toRaster)
        gfx->endWrite();
    free(errCur);
    free(errNext);
}

/**
 * JPEGDEC draw callback — receives 8-bit grayscale MCU blocks and renders them
 * to the Inkplate display. Replicates the Inkplate SDK's Floyd-Steinberg
 * dithering (including the 1-bit luminance halving that biases the image
 * darker) so output matches the previous TJpgDec-based implementation.
 * Relies on setMaxOutputSize(1) so each block is a single MCU (<= 16px wide).
 */
int ImageScaler::jpegDrawCallback(JPEGDRAW *pDraw)
{
    if (!s_display)
        return 0;

    const int16_t x = pDraw->x;
    const int16_t y = pDraw->y;
    const int w = pDraw->iWidthUsed; // visible width (clipped at image edge)
    const int h = pDraw->iHeight;
    const int pitch = pDraw->iWidth; // full decoded block stride
    uint8_t *pixels = (uint8_t *)pDraw->pPixels;

    // Capture mode: copy decoded grayscale into the PSRAM framebuffer for
    // later high-quality software downscaling (no drawing to the display).
    if (s_captureBuf)
    {
        for (int j = 0; j < h; ++j)
        {
            int gy = y + j;
            if (gy < 0 || gy >= s_captureH)
                continue;
            uint8_t *dst = s_captureBuf + (size_t)gy * s_captureW;
            const uint8_t *src = pixels + (size_t)j * pitch;
            for (int i = 0; i < w; ++i)
            {
                int gx = x + i;
                if (gx >= 0 && gx < s_captureW)
                    dst[gx] = src[i];
            }
        }
        return 1;
    }

    bool is1Bit = (s_display->getDisplayMode() == INKPLATE_1BIT);
    Adafruit_GFX *gfx = static_cast<Adafruit_GFX *>(s_display);

    // Dither row transition: carry the accumulated error down to the next
    // block-row and clear the working row.
    if (s_dither && y != s_lastY)
    {
        for (int i = 0; i < E_INK_WIDTH + 20; ++i)
        {
            s_ditherBuffer[0][i] = s_ditherBuffer[1][i];
            s_ditherBuffer[1][i] = 0;
        }
        s_lastY = y;
    }

    gfx->startWrite();

    for (int j = 0; j < h; ++j)
    {
        for (int i = 0; i < w; ++i)
        {
            uint8_t px = pixels[j * pitch + i]; // 8-bit luminance (0=black, 255=white)
            uint32_t val;

            if (s_dither)
            {
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
                // No dithering: quantize luminance directly to 3-bit
                val = px >> 5;
            }

            if (is1Bit)
            {
                val = (~val >> 2) & 1;
                if (s_invert)
                    val = 1 - val;
            }
            else
            {
                if (s_invert)
                    val = 7 - val;
            }

            gfx->writePixel(x + i, y + j, val);
        }
    }

    // Propagate dithering errors between horizontal blocks
    if (s_dither)
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
    return 1;
}

// ---------------------------------------------------------------------------
// Cover thumbnail cache
// ---------------------------------------------------------------------------

String ImageScaler::deriveCachePath(const char *srcPath, uint8_t scaleFactor)
{
    String p = String(srcPath);
    int dot = p.lastIndexOf('.');
    int slash = p.lastIndexOf('/');
    String base = (dot > slash) ? p.substring(0, dot) : p;
    // Distinct .pdt extension so nothing mistakes the raster for a decodable JPEG.
    return base + "_" + String(scaleFactor) + ".pdt";
}

bool ImageScaler::isProgressiveJpeg(const char *path)
{
    SdFile f;
    if (!f.open(path, O_RDONLY))
        return false;

    // Scan JPEG markers for a Start-Of-Frame; SOF2 (0xC2) == progressive.
    bool progressive = false;
    uint8_t hdr[2];
    if (f.read(hdr, 2) == 2 && hdr[0] == 0xFF && hdr[1] == 0xD8)
    {
        uint8_t m[2];
        while (f.read(m, 2) == 2)
        {
            if (m[0] != 0xFF)
                break;
            uint8_t marker = m[1];
            if (marker == 0xC2)
            {
                progressive = true;
                break;
            }
            // Baseline / other SOF -> not progressive
            if (marker == 0xC0 || marker == 0xC1 || marker == 0xC3 || marker == 0xDA)
                break;
            uint8_t len[2];
            if (f.read(len, 2) != 2)
                break;
            int seglen = (len[0] << 8) | len[1];
            f.seekCur(seglen - 2);
        }
    }
    f.close();
    return progressive;
}

uint8_t *ImageScaler::decodeJpegToGray(const char *path, uint8_t reqScale,
                                       int &decW, int &decH, bool &isProgressive)
{
    SdFile dat;
    if (!dat.open(path, O_RDONLY))
    {
        Serial.printf("[ImageScaler] Failed to open: %s\n", path);
        return nullptr;
    }

    uint32_t total = dat.fileSize();
    uint8_t *buff = (uint8_t *)ps_malloc(total);
    if (!buff)
    {
        Serial.println("[ImageScaler] ps_malloc failed for file buffer");
        dat.close();
        return nullptr;
    }

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

    JPEGDEC *jpeg = new JPEGDEC();
    if (!jpeg || !jpeg->openRAM(buff, total, jpegDrawCallback))
    {
        Serial.println("[ImageScaler] decodeJpegToGray: openRAM failed");
        if (jpeg) delete jpeg;
        free(buff);
        return nullptr;
    }

    isProgressive = (jpeg->getJPEGType() == JPEG_MODE_PROGRESSIVE);

    int decOptions = 0;
    switch (reqScale)
    {
    case 2: decOptions = JPEG_SCALE_HALF; break;
    case 4: decOptions = JPEG_SCALE_QUARTER; break;
    case 8: decOptions = JPEG_SCALE_EIGHTH; break;
    default: break;
    }

    decW = (jpeg->getWidth() + reqScale - 1) / reqScale;
    decH = (jpeg->getHeight() + reqScale - 1) / reqScale;

    uint8_t *fb = (uint8_t *)ps_malloc((size_t)decW * decH);
    if (!fb)
    {
        Serial.println("[ImageScaler] ps_malloc failed for grayscale buffer");
        jpeg->close();
        delete jpeg;
        free(buff);
        return nullptr;
    }

    // Capture mode: callback copies decoded grayscale into fb at (0,0).
    s_display = _display;
    s_captureBuf = fb;
    s_captureW = decW;
    s_captureH = decH;
    jpeg->setPixelType(EIGHT_BIT_GRAYSCALE);
    jpeg->setMaxOutputSize(1);
    int ok = jpeg->decode(0, 0, decOptions);
    s_captureBuf = nullptr;

    jpeg->close();
    delete jpeg;
    free(buff);

    if (!ok)
    {
        Serial.println("[ImageScaler] decodeJpegToGray: decode failed");
        free(fb);
        return nullptr;
    }
    return fb;
}

bool ImageScaler::writeScaledCache(const uint8_t *gray, int gw, int gh,
                                   int outW, int outH, const char *dstPath)
{
    if (outW <= 0 || outH <= 0)
        return false;

    const bool is1Bit = (_display->getDisplayMode() == INKPLATE_1BIT);
    const uint8_t bitDepth = is1Bit ? 1 : 3;
    const int pitch = is1Bit ? ((outW + 7) >> 3) : ((outW + 1) >> 1);
    const size_t payload = (size_t)pitch * outH;
    const size_t fileLen = PDT_HEADER_SIZE + payload;

    uint8_t *out = (uint8_t *)ps_malloc(fileLen);
    if (!out)
    {
        Serial.println("[ImageScaler] ps_malloc failed for cache buffer");
        return false;
    }
    memset(out, 0, fileLen);

    // Header
    memcpy(out, PDT_MAGIC, 4);
    out[4] = bitDepth;
    out[5] = 0;
    out[6] = (uint8_t)(outW & 0xFF);
    out[7] = (uint8_t)((outW >> 8) & 0xFF);
    out[8] = (uint8_t)(outH & 0xFF);
    out[9] = (uint8_t)((outH >> 8) & 0xFF);
    out[10] = 0;
    out[11] = 0;

    // Area-average + dither directly into the payload region.
    ditherBlitDownscaled(gray, gw, gh, outW, outH, 0, 0, true, false,
                         out + PDT_HEADER_SIZE);

    bool ok = sdHandler.saveFile(String(dstPath), (const char *)out, fileLen);
    free(out);
    if (!ok)
        Serial.printf("[ImageScaler] Failed to write cache: %s\n", dstPath);
    return ok;
}

bool ImageScaler::generateScaledCache(const char *srcPath, uint8_t scaleFactor)
{
    if (!isJpeg(srcPath))
        return false;

    int origW, origH;
    if (!sdHandler.getImageDimensions(String(srcPath), origW, origH))
        return false;

    const int outW = (origW + scaleFactor - 1) / scaleFactor;
    const int outH = (origH + scaleFactor - 1) / scaleFactor;
    String dst = deriveCachePath(srcPath, scaleFactor);

    if (!isProgressiveJpeg(srcPath))
    {
        // Baseline: decode at full IDCT (memory-capped) and area-average to the
        // target size — sharp, matches the on-screen quality.
        const uint32_t CAP_PIXELS = 1500000;
        uint8_t decScale = 1;
        int decW = origW, decH = origH;
        while ((uint32_t)decW * decH > CAP_PIXELS)
        {
            decScale <<= 1;
            decW = (origW + decScale - 1) / decScale;
            decH = (origH + decScale - 1) / decScale;
            if (decScale >= 8)
                break;
        }

        bool prog;
        uint8_t *fb = decodeJpegToGray(srcPath, decScale, decW, decH, prog);
        if (!fb)
            return false;

        bool ok = writeScaledCache(fb, decW, decH, outW, outH, dst.c_str());
        free(fb);
        return ok;
    }
    else
    {
        // Progressive: JPEGDEC needs large internal buffers, so avoid a big
        // full-resolution framebuffer. Decode directly at the target scale
        // (DC-only quality, same as the on-screen result) into a small buffer.
        Serial.println("[ImageScaler] Caching progressive cover (DC-only quality)");
        int decW, decH;
        bool prog;
        uint8_t *fb = decodeJpegToGray(srcPath, scaleFactor, decW, decH, prog);
        if (!fb)
            return false;

        // decW/decH already equal the target size (decoded at this scale).
        bool ok = writeScaledCache(fb, decW, decH, decW, decH, dst.c_str());
        free(fb);
        return ok;
    }
}

bool ImageScaler::drawCachedBitmap(const char *cachePath, int x, int y)
{
    SdFile f;
    if (!f.open(cachePath, O_RDONLY))
        return false;

    uint8_t hdr[PDT_HEADER_SIZE];
    if (f.read(hdr, PDT_HEADER_SIZE) != PDT_HEADER_SIZE ||
        memcmp(hdr, PDT_MAGIC, 4) != 0)
    {
        f.close();
        return false;
    }

    uint8_t bitDepth = hdr[4];
    int w = hdr[6] | (hdr[7] << 8);
    int h = hdr[8] | (hdr[9] << 8);

    const bool is1Bit = (_display->getDisplayMode() == INKPLATE_1BIT);
    // Cache must match the current display bit-depth (baked at generation time).
    if (bitDepth != (is1Bit ? 1 : 3) || w <= 0 || h <= 0)
    {
        f.close();
        return false;
    }

    const int pitch = is1Bit ? ((w + 7) >> 3) : ((w + 1) >> 1);
    uint8_t *rowBuf = (uint8_t *)ps_malloc(pitch);
    if (!rowBuf)
    {
        f.close();
        return false;
    }

    Adafruit_GFX *gfx = static_cast<Adafruit_GFX *>(_display);
    gfx->startWrite();
    for (int row = 0; row < h; ++row)
    {
        if (f.read(rowBuf, pitch) != pitch)
            break;
        for (int col = 0; col < w; ++col)
        {
            uint8_t val;
            if (is1Bit)
                val = (rowBuf[col >> 3] >> (7 - (col & 7))) & 1;
            else
                val = (col & 1) ? (rowBuf[col >> 1] & 0x0F)
                                : ((rowBuf[col >> 1] >> 4) & 0x0F);
            gfx->writePixel(x + col, y + row, val);
        }
    }
    gfx->endWrite();

    free(rowBuf);
    f.close();
    return true;
}
