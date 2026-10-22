// FlickBook digital twin - Inkplate 6 FLICK board driver model and panel.
#include "Inkplate.h"

#include <cstdio>
#include <cstring>

#include "panel.h"
#include "twin.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

using namespace twin;

// =========================================================================
// Panel
// =========================================================================
namespace twin
{
namespace panel
{
namespace
{
std::vector<uint8_t> g_native(kWidth * kHeight, 255);
uint8_t g_rotation = 0;
uint64_t g_frame = 0;
const uint8_t **g_fb1 = nullptr;
const uint8_t **g_fb3 = nullptr;
const uint8_t *g_mode = nullptr;
} // namespace

const std::vector<uint8_t> &native() { return g_native; }
std::vector<uint8_t> &nativeMutable() { return g_native; }
void setRotation(uint8_t r) { g_rotation = r & 3; }
uint8_t rotation() { return g_rotation; }
uint64_t frameCounter() { return g_frame; }
void bumpFrame() { ++g_frame; }

void registerFramebuffer(const uint8_t **buf1b, const uint8_t **buf3b, const uint8_t *mode)
{
    g_fb1 = buf1b;
    g_fb3 = buf3b;
    g_mode = mode;
}

// Inverse of EPDDriver::writePixelInternal's rotation transform.
void nativeToLogical(int rx, int ry, int &lx, int &ly)
{
    switch (g_rotation)
    {
    case 1: // raw_x = W-1-ly, raw_y = lx
        lx = ry;
        ly = kWidth - 1 - rx;
        break;
    case 2:
        lx = kWidth - 1 - rx;
        ly = kHeight - 1 - ry;
        break;
    case 3: // raw_x = ly, raw_y = H-1-lx
        lx = kHeight - 1 - ry;
        ly = rx;
        break;
    default:
        lx = rx;
        ly = ry;
        break;
    }
}

static Image rotateNative(const std::vector<uint8_t> &src)
{
    Image img;
    bool portrait = (g_rotation & 1) != 0;
    img.w = portrait ? kHeight : kWidth;
    img.h = portrait ? kWidth : kHeight;
    img.gray.resize((size_t)img.w * img.h);
    for (int ry = 0; ry < kHeight; ++ry)
    {
        for (int rx = 0; rx < kWidth; ++rx)
        {
            int lx, ly;
            nativeToLogical(rx, ry, lx, ly);
            img.gray[(size_t)ly * img.w + lx] = src[(size_t)ry * kWidth + rx];
        }
    }
    return img;
}

Image logical() { return rotateNative(g_native); }

Image framebuffer()
{
    std::vector<uint8_t> raw(kWidth * kHeight, 255);
    if (g_mode && *g_mode == 0 && g_fb1 && *g_fb1)
    {
        const uint8_t *b = *g_fb1;
        for (int i = 0; i < kWidth * kHeight; ++i)
            raw[i] = (b[i >> 3] >> (i & 7)) & 1 ? 0 : 255;
    }
    else if (g_mode && *g_mode == 1 && g_fb3 && *g_fb3)
    {
        const uint8_t *b = *g_fb3;
        for (int i = 0; i < kWidth * kHeight; ++i)
        {
            uint8_t v = (i & 1) ? (b[i >> 1] & 0x0F) : (b[i >> 1] >> 4);
            raw[i] = (uint8_t)((v & 7) * 255 / 7);
        }
    }
    return rotateNative(raw);
}

std::vector<uint8_t> logicalRgb(const Image &img, bool frontlight)
{
    std::vector<uint8_t> rgb((size_t)img.w * img.h * 3);
    const auto &p = peripherals();
    float level = (frontlight && p.frontlight_on) ? p.frontlight_level / 63.0f : 0.0f;
    for (size_t i = 0; i < img.gray.size(); ++i)
    {
        float g = img.gray[i];
        // Unlit e-paper is a slightly dim paper gray; the frontlight adds a
        // warm glow proportional to its level.
        float r = g * (0.92f + 0.08f * level) + 18.0f * level;
        float gg = g * (0.92f + 0.07f * level) + 12.0f * level;
        float b = g * (0.90f + 0.02f * level) + 2.0f * level;
        if (!frontlight)
            r = gg = b = g;
        rgb[i * 3 + 0] = (uint8_t)std::min(255.0f, r);
        rgb[i * 3 + 1] = (uint8_t)std::min(255.0f, gg);
        rgb[i * 3 + 2] = (uint8_t)std::min(255.0f, b);
    }
    return rgb;
}

uint64_t hash(const Image &img)
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint8_t b) {
        h ^= b;
        h *= 1099511628211ull;
    };
    for (int i = 0; i < 4; ++i)
        mix((uint8_t)(img.w >> (8 * i)));
    for (int i = 0; i < 4; ++i)
        mix((uint8_t)(img.h >> (8 * i)));
    for (uint8_t b : img.gray)
        mix(b);
    return h;
}

std::string hex64(uint64_t v)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)v);
    return buf;
}

static void appendBytes(void *ctx, void *data, int size)
{
    auto *out = static_cast<std::vector<uint8_t> *>(ctx);
    out->insert(out->end(), (uint8_t *)data, (uint8_t *)data + size);
}

std::vector<uint8_t> encodePng(const Image &img)
{
    std::vector<uint8_t> out;
    stbi_write_png_to_func(appendBytes, &out, img.w, img.h, 1, img.gray.data(), img.w);
    return out;
}

std::vector<uint8_t> encodePngRgb(int w, int h, const std::vector<uint8_t> &rgb)
{
    std::vector<uint8_t> out;
    stbi_write_png_to_func(appendBytes, &out, w, h, 3, rgb.data(), w * 3);
    return out;
}

bool writePng(const std::string &path, const Image &img)
{
    auto data = encodePng(img);
    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
        return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    return ok;
}
} // namespace panel
} // namespace twin

// =========================================================================
// Board driver model
// =========================================================================
namespace
{
constexpr size_t kBuf1 = E_INK_WIDTH * E_INK_HEIGHT / 8;
constexpr size_t kBuf3 = E_INK_WIDTH * E_INK_HEIGHT / 2;
// Extra bytes after each framebuffer to detect overruns. On the device the
// framebuffers are exactly kBuf1/kBuf3 bytes; anything written past them
// corrupts unrelated memory.
constexpr size_t kGuard = 16384;
constexpr uint8_t kGuardByte = 0xA5;

SdFat g_sd;
SPIClass g_spi2(2);

uint8_t *allocGuarded(size_t n, uint8_t fill)
{
    uint8_t *p = (uint8_t *)malloc(n + kGuard);
    memset(p, fill, n);
    memset(p + n, kGuardByte, kGuard);
    return p;
}

bool checkGuard(uint8_t *buf, size_t n, const char *name)
{
    if (!buf)
        return true;
    size_t last = 0;
    for (size_t i = 0; i < kGuard; ++i)
        if (buf[n + i] != kGuardByte)
            last = i + 1;
    if (!last)
        return true;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "framebuffer overrun: firmware wrote at least %zu byte(s) past %s (size %zu bytes). "
             "On the device this corrupts adjacent memory.",
             last, name, n);
    static std::string lastMsg;
    if (lastMsg != msg)
    {
        twin::warn(msg);
        lastMsg = msg;
    }
    memset(buf + n, kGuardByte, kGuard);
    return false;
}

inline uint8_t bit1(const uint8_t *b, int i) { return (b[i >> 3] >> (i & 7)) & 1; }

struct Bbox
{
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    void add(int x, int y)
    {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
    }
};

void finishEvent(RefreshEvent &e, const Bbox &bb)
{
    if (bb.x1 >= 0)
    {
        e.x0 = bb.x0;
        e.y0 = bb.y0;
        e.x1 = bb.x1;
        e.y1 = bb.y1;
    }
}

// Apply a full refresh of a 1-bit framebuffer to the panel.
void panelFull1b(const uint8_t *buf, RefreshEvent &e)
{
    auto &p = panel::nativeMutable();
    Bbox bb;
    for (int i = 0; i < E_INK_WIDTH * E_INK_HEIGHT; ++i)
    {
        uint8_t v = bit1(buf, i) ? 0 : 255;
        if (p[i] != v)
        {
            p[i] = v;
            ++e.changed;
            int lx, ly;
            panel::nativeToLogical(i % E_INK_WIDTH, i / E_INK_WIDTH, lx, ly);
            bb.add(lx, ly);
        }
    }
    finishEvent(e, bb);
}

void panelFull3b(const uint8_t *buf, RefreshEvent &e)
{
    auto &p = panel::nativeMutable();
    Bbox bb;
    for (int i = 0; i < E_INK_WIDTH * E_INK_HEIGHT; ++i)
    {
        uint8_t raw = (i & 1) ? (buf[i >> 1] & 0x0F) : (buf[i >> 1] >> 4);
        uint8_t v = (uint8_t)((raw & 7) * 255 / 7);
        if (p[i] != v)
        {
            p[i] = v;
            ++e.changed;
            int lx, ly;
            panel::nativeToLogical(i % E_INK_WIDTH, i / E_INK_WIDTH, lx, ly);
            bb.add(lx, ly);
        }
    }
    finishEvent(e, bb);
}

// Partial update: only pixels that differ between the previously shown
// buffer and the new one are driven. With ghosting enabled, pixels driven
// black -> white keep a faint residue until the next full refresh.
void panelPartial1b(const uint8_t *oldBuf, const uint8_t *newBuf, RefreshEvent &e)
{
    auto &p = panel::nativeMutable();
    const bool ghost = config().ghosting;
    Bbox bb;
    for (int i = 0; i < E_INK_WIDTH * E_INK_HEIGHT; ++i)
    {
        uint8_t o = bit1(oldBuf, i), n = bit1(newBuf, i);
        if (o == n)
            continue;
        uint8_t v = n ? 0 : (ghost ? 238 : 255);
        if (p[i] != v)
        {
            p[i] = v;
            ++e.changed;
            int lx, ly;
            panel::nativeToLogical(i % E_INK_WIDTH, i / E_INK_WIDTH, lx, ly);
            bb.add(lx, ly);
        }
    }
    finishEvent(e, bb);
}
} // namespace

// ------------------------------------------------------------------ Touch ---
bool Touch::init(uint8_t pwrState)
{
    _power = pwrState;
    _ready = config().touch_init_ok;
    return _ready;
}

bool Touch::available() { return _ready && twin::touchAvailable(); }

uint8_t Touch::getData(uint16_t *xPos, uint16_t *yPos, uint8_t *z)
{
    if (!xPos || !yPos)
        return 0;
    if (z)
        z[0] = z[1] = 0;
    if (!_ready)
    {
        xPos[0] = xPos[1] = yPos[0] = yPos[1] = 0;
        return 0;
    }
    uint8_t n = twin::touchRead(xPos, yPos);
    if (z)
        for (int i = 0; i < n; ++i)
            z[i] = 40;
    return n;
}

bool Touch::touchInArea(int16_t x1, int16_t y1, int16_t w, int16_t h)
{
    uint16_t x[2], y[2];
    if (!available())
        return false;
    uint8_t n = getData(x, y);
    return n && x[0] >= x1 && x[0] < x1 + w && y[0] >= y1 && y[0] < y1 + h;
}

// ------------------------------------------------------------- Frontlight ---
void Frontlight::setBrightness(uint8_t v)
{
    peripherals().frontlight_level = v & 63;
    panel::bumpFrame();
}

void Frontlight::setState(bool e)
{
    peripherals().frontlight_on = e;
    panel::bumpFrame();
}

// -------------------------------------------------------------- EPDDriver ---
int EPDDriver::initDriver(Inkplate *inkplatePtr)
{
    if (_beginDone)
        return 0;
    _inkplate = inkplatePtr;
    image.begin(inkplatePtr);
    touchscreen.begin(inkplatePtr);
    frontlight.begin(inkplatePtr);
    if (!_partial)
    {
        DMemoryNew = allocGuarded(kBuf1, 0);
        _partial = allocGuarded(kBuf1, 0);
        DMemory4Bit = allocGuarded(kBuf3, 255);
    }
    panel::registerFramebuffer(const_cast<const uint8_t **>(&_partial), const_cast<const uint8_t **>(&DMemory4Bit),
                               &_displayMode);
    _beginDone = 1;
    return 1;
}

bool EPDDriver::twinCheckGuards()
{
    bool ok = checkGuard(_partial, kBuf1, "_partial (1-bit framebuffer)");
    ok &= checkGuard(DMemoryNew, kBuf1, "DMemoryNew");
    ok &= checkGuard(DMemory4Bit, kBuf3, "DMemory4Bit (3-bit framebuffer)");
    return ok;
}

void EPDDriver::writePixelInternal(int16_t x, int16_t y, uint16_t color)
{
    int16_t x0 = x;
    int16_t y0 = y;
    if (x0 > _inkplate->width() - 1 || y0 > _inkplate->height() - 1 || x0 < 0 || y0 < 0)
        return;

    switch (_inkplate->getRotation())
    {
    case 1:
        _swap_int16_t(x0, y0);
        x0 = _inkplate->height() - x0 - 1;
        break;
    case 2:
        x0 = _inkplate->width() - x0 - 1;
        y0 = _inkplate->height() - y0 - 1;
        break;
    case 3:
        _swap_int16_t(x0, y0);
        y0 = _inkplate->width() - y0 - 1;
        break;
    }

    if (_displayMode == 0)
    {
        int bx = x0 / 8;
        int x_sub = x0 % 8;
        uint8_t temp = *(_partial + (E_INK_WIDTH / 8 * y0) + bx);
        *(_partial + (E_INK_WIDTH / 8 * y0) + bx) = (~pixelMaskLUT[x_sub] & temp) | (color ? pixelMaskLUT[x_sub] : 0);
    }
    else
    {
        color &= 7;
        int bx = x0 / 2;
        int x_sub = x0 % 2;
        uint8_t temp = *(DMemory4Bit + E_INK_WIDTH / 2 * y0 + bx);
        *(DMemory4Bit + E_INK_WIDTH / 2 * y0 + bx) = (pixelMaskGLUT[x_sub] & temp) | (x_sub ? color : color << 4);
    }
}

void EPDDriver::clearDisplay()
{
    if (_displayMode == 0)
        memset(_partial, 0, kBuf1);
    else if (_displayMode == 1)
        memset(DMemory4Bit, 255, kBuf3);
}

void EPDDriver::display(bool)
{
    twinCheckGuards();
    if (_inkplate)
        panel::setRotation(_inkplate->getRotation());
    RefreshEvent e;
    e.t = nowMs();
    e.kind = "full";
    e.mode = _displayMode;
    if (_displayMode == 0)
    {
        memcpy(DMemoryNew, _partial, kBuf1);
        panelFull1b(DMemoryNew, e);
        _blockPartial = 0;
    }
    else
    {
        panelFull3b(DMemory4Bit, e);
    }
    panel::bumpFrame();
    recordRefresh(e);
    if (config().simulate_refresh_time)
        advanceMs(_displayMode == 0 ? config().full_refresh_ms : config().full_refresh_3bit_ms);
}

uint32_t EPDDriver::partialUpdate(bool forced, bool)
{
    twinCheckGuards();
    if (_inkplate)
        panel::setRotation(_inkplate->getRotation());
    RefreshEvent e;
    e.t = nowMs();
    e.mode = _displayMode;
    if (_displayMode == 1)
    {
        // The real driver silently ignores partial updates in 3-bit mode.
        e.kind = "partial_ignored_3bit";
        recordRefresh(e);
        return 0;
    }
    if ((_blockPartial == 1 && !forced) ||
        (_partialUpdateCounter >= _partialUpdateLimiter && _partialUpdateLimiter != 0))
    {
        bool limiter = !(_blockPartial == 1 && !forced);
        e.kind = "partial_promoted_full";
        memcpy(DMemoryNew, _partial, kBuf1);
        panelFull1b(DMemoryNew, e);
        _blockPartial = 0;
        if (limiter)
            _partialUpdateCounter = 0;
        panel::bumpFrame();
        recordRefresh(e);
        if (config().simulate_refresh_time)
            advanceMs(config().full_refresh_ms);
        return 0;
    }

    e.kind = "partial";
    // Count pixels turning black -> white (the driver's return value).
    uint32_t changeCount = 0;
    for (size_t i = 0; i < kBuf1; ++i)
    {
        uint8_t diffw = DMemoryNew[i] & ~_partial[i];
        changeCount += (uint32_t)__builtin_popcount(diffw);
    }
    panelPartial1b(DMemoryNew, _partial, e);
    memcpy(DMemoryNew, _partial, kBuf1);
    if (_partialUpdateLimiter != 0)
        _partialUpdateCounter++;
    panel::bumpFrame();
    recordRefresh(e);
    if (config().simulate_refresh_time)
        advanceMs(config().partial_refresh_ms);
    return changeCount;
}

void EPDDriver::setFullUpdateThreshold(uint16_t n)
{
    _partialUpdateLimiter = n;
    if (n != 0)
        _blockPartial = 1;
}

int16_t EPDDriver::sdCardInit()
{
    delay(50);
    setSdCardOk(g_sd.begin(SdSpiConfig(15, SHARED_SPI, SD_SCK_MHZ(25), &g_spi2)));
    if (!getSdCardOk())
        twin::serialLogMark("SD card not available (root '%s', present=%d)", config().sd_root.c_str(),
                            (int)config().sd_present);
    return getSdCardOk();
}

SdFat &EPDDriver::getSdFat() { return g_sd; }
SPIClass *EPDDriver::getSPIptr() { return &g_spi2; }

double EPDDriver::readBattery()
{
    delay(5);
    return peripherals().battery_v;
}

// --------------------------------------------------------------- Inkplate ---
Inkplate::Inkplate(uint8_t mode) : Adafruit_GFX(E_INK_WIDTH, E_INK_HEIGHT), Graphics(E_INK_WIDTH, E_INK_HEIGHT)
{
    _mode = mode;
}

void Inkplate::begin()
{
    if (_beginDone == 1)
        return;
    initDriver(this);
    selectDisplayMode(_mode);
    clearDisplay();
    _beginDone = 1;
}

void Inkplate::preloadScreen() { memcpy(DMemoryNew, _partial, kBuf1); }

void Inkplate::drawPixel(int16_t x, int16_t y, uint16_t color) { writePixel(x, y, color); }

void Inkplate::writePixel(int16_t x, int16_t y, uint16_t color) { writePixelInternal(x, y, color); }

void Inkplate::setRotation(uint8_t r)
{
    rotation = (r & 3);
    switch (rotation)
    {
    case 0:
    case 2:
        _width = E_INK_WIDTH;
        _height = E_INK_HEIGHT;
        break;
    case 1:
    case 3:
        _width = E_INK_HEIGHT;
        _height = E_INK_WIDTH;
        break;
    }
}
