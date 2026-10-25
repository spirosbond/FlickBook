// FlickBook digital twin - e-paper panel model.
//
// The panel holds what is physically visible (8-bit gray, 0 = black,
// 255 = white) in the native (unrotated) 1024x758 orientation. It only changes
// when the firmware calls display()/partialUpdate(), exactly like the real
// e-paper, so screenshots never show half-drawn framebuffers.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace twin
{
namespace panel
{
constexpr int kWidth = 1024; // E_INK_WIDTH
constexpr int kHeight = 758; // E_INK_HEIGHT

struct Image
{
    int w = 0, h = 0;
    std::vector<uint8_t> gray; // w*h, row-major
};

// Native-orientation panel pixels.
const std::vector<uint8_t> &native();
std::vector<uint8_t> &nativeMutable();

// Rotation used to present the panel (follows the firmware's setRotation()).
void setRotation(uint8_t r);
uint8_t rotation();

// Panel as the user sees it (rotated into logical coordinates). When
// `frontlight` is true, the current frontlight level is applied as a warm
// overlay (RGB output in `rgb`, 3 bytes per pixel).
Image logical();
std::vector<uint8_t> logicalRgb(const Image &img, bool frontlight);

// Framebuffer contents not necessarily visible yet (what the firmware drew),
// in logical coordinates. Requires a registered display.
Image framebuffer();
void registerFramebuffer(const uint8_t **buf1b, const uint8_t **buf3b, const uint8_t *mode);

// Map native (raw) coordinates to logical coordinates for the current rotation.
void nativeToLogical(int rx, int ry, int &lx, int &ly);

uint64_t frameCounter(); // increments every time the panel changes
void bumpFrame();

// Visible phases of a full refresh (viewer-only; the panel itself changes
// atomically). level: 255 = white, 0 = black, -1 = the new image.
struct FlashPhase
{
    int level;
    uint32_t ms;
};
// Phases for display mode 0 (1-bit) or 1 (3-bit), summing to the configured
// full-refresh time. Empty when the flash is disabled.
std::vector<FlashPhase> fullRefreshPhases(uint8_t mode);
std::string fullRefreshPhasesJson(); // {"1bit":[[level,ms],...],"3bit":[...]} or "null"
// Called during a full refresh, before viewers show the new image, with the
// phases preceding it. Viewers use it to show the flash live (SDL window).
using FlashPresenter = std::function<void(const std::vector<FlashPhase> &)>;
void setFlashPresenter(FlashPresenter presenter);
const FlashPresenter &flashPresenter();

uint64_t hash(const Image &img); // FNV-1a 64 of the gray pixels + dimensions
std::string hex64(uint64_t v);

// Encode helpers.
bool writePng(const std::string &path, const Image &img);
std::vector<uint8_t> encodePng(const Image &img);
std::vector<uint8_t> encodePngRgb(int w, int h, const std::vector<uint8_t> &rgb);
} // namespace panel
} // namespace twin
