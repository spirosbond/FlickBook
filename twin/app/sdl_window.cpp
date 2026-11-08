// FlickBook digital twin - interactive SDL2 window.
//
// Mouse: press/drag/release are forwarded as live touch samples (so taps,
// long presses and swipes behave like a finger). Keys:
//   Left/Right/Up/Down  swipe in that direction (finger movement)
//   L                   toggle frontlight overlay rendering
//   F                   show framebuffer instead of panel (debug)
//   [ / ]               battery voltage -/+ 0.1 V
//   S                   save screenshot (flicktwin-<time>.png)
//   R                   reset (restart firmware with a fresh SD card)
//   Esc / Q             quit
#include <SDL.h>

#include <algorithm>
#include <string>
#include <vector>

#include "agent.h"
#include "http.h"
#include "panel.h"
#include "runtime.h"
#include "twin.h"

void pollHttpAgent(twin::http::Listener &l, twin::agent::Control &ctl);

using namespace twin;

int runSdlWindow(float scale, bool &resetRequested, http::Listener *httpAgent)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }
    panel::Image img = panel::logical();
    int ww = (int)(img.w * scale), wh = (int)(img.h * scale);
    SDL_Window *win = SDL_CreateWindow("FlickBook twin", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh,
                                       SDL_WINDOW_RESIZABLE);
    if (!win)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
    SDL_Texture *tex = nullptr;
    int texW = 0, texH = 0;
    uint64_t shownFrame = ~0ull;
    bool showLight = true, showFb = false, mouseDown = false;
    uint64_t lastMoveMs = 0;
    agent::Control ctl;

    auto toPanel = [&](int mx, int my, int &px, int &py) {
        int cw, ch;
        SDL_GetWindowSize(win, &cw, &ch);
        px = std::max(0, std::min(texW - 1, mx * texW / std::max(1, cw)));
        py = std::max(0, std::min(texH - 1, my * texH / std::max(1, ch)));
    };
    auto press = [&](int px, int py, uint8_t fingers) {
        TouchSample s;
        s.t = nowMs();
        s.fingers = fingers;
        s.x[0] = (uint16_t)px;
        s.y[0] = (uint16_t)py;
        touchEnqueue(s);
    };

    // Full refreshes call this before the new image is applied; input that
    // arrives meanwhile stays queued until the refresh completes.
    panel::setFlashPresenter([&](const std::vector<panel::FlashPhase> &phases) {
        if (!tex || showFb)
            return;
        panel::Image solid;
        solid.w = texW;
        solid.h = texH;
        for (const auto &ph : phases)
        {
            solid.gray.assign((size_t)texW * texH, (uint8_t)ph.level);
            std::vector<uint8_t> rgb = panel::logicalRgb(solid, showLight);
            SDL_UpdateTexture(tex, nullptr, rgb.data(), texW * 3);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
            const uint32_t end = SDL_GetTicks() + ph.ms;
            for (uint32_t now = SDL_GetTicks(); now < end; now = SDL_GetTicks())
            {
                SDL_PumpEvents();
                SDL_Delay(std::min<uint32_t>(10, end - now));
            }
        }
        shownFrame = ~0ull;
    });

    bool running = true;
    while (running && !ctl.quit && !ctl.reset)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                running = false;
            else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT)
            {
                int px, py;
                toPanel(ev.button.x, ev.button.y, px, py);
                mouseDown = true;
                press(px, py, 1);
                lastMoveMs = nowMs();
            }
            else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT && mouseDown)
            {
                mouseDown = false;
                press(0, 0, 0);
            }
            else if (ev.type == SDL_MOUSEMOTION && mouseDown && nowMs() - lastMoveMs >= kTouchReportIntervalMs)
            {
                int px, py;
                toPanel(ev.motion.x, ev.motion.y, px, py);
                press(px, py, 1);
                lastMoveMs = nowMs();
            }
            else if (ev.type == SDL_KEYDOWN)
            {
                const int cx = texW / 2, cy = texH / 2, d = 150;
                switch (ev.key.keysym.sym)
                {
                case SDLK_LEFT: gestureSwipe(cx + d, cy, cx - d, cy); break;
                case SDLK_RIGHT: gestureSwipe(cx - d, cy, cx + d, cy); break;
                case SDLK_UP: gestureSwipe(cx, cy + d, cx, cy - d); break;
                case SDLK_DOWN: gestureSwipe(cx, cy - d, cx, cy + d); break;
                case SDLK_l: showLight = !showLight; shownFrame = ~0ull; break;
                case SDLK_f: showFb = !showFb; shownFrame = ~0ull; break;
                case SDLK_LEFTBRACKET: peripherals().battery_v -= 0.1; break;
                case SDLK_RIGHTBRACKET: peripherals().battery_v += 0.1; break;
                case SDLK_s:
                {
                    std::string path = "flicktwin-" + std::to_string(nowMs()) + ".png";
                    panel::writePng(path, showFb ? panel::framebuffer() : panel::logical());
                    serialLogMark("screenshot saved to %s", path.c_str());
                    break;
                }
                case SDLK_r: ctl.reset = true; break;
                case SDLK_ESCAPE:
                case SDLK_q: running = false; break;
                default: break;
                }
                if (ev.key.keysym.sym == SDLK_LEFTBRACKET || ev.key.keysym.sym == SDLK_RIGHTBRACKET)
                    serialLogMark("battery set to %.2f V", peripherals().battery_v);
            }
        }
        // Keep the finger "down" while the mouse is held still (the firmware
        // only detects long presses on repeated press reports).
        if (mouseDown && nowMs() - lastMoveMs >= kTouchReportIntervalMs)
        {
            int mx, my, px, py;
            SDL_GetMouseState(&mx, &my);
            toPanel(mx, my, px, py);
            press(px, py, 1);
            lastMoveMs = nowMs();
        }

        if (httpAgent)
            pollHttpAgent(*httpAgent, ctl);
        runtime::step();

        if (panel::frameCounter() != shownFrame || showFb)
        {
            shownFrame = panel::frameCounter();
            img = showFb ? panel::framebuffer() : panel::logical();
            if (!tex || texW != img.w || texH != img.h)
            {
                if (tex)
                    SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, img.w, img.h);
                texW = img.w;
                texH = img.h;
                SDL_SetWindowSize(win, (int)(img.w * scale), (int)(img.h * scale));
            }
            std::vector<uint8_t> rgb = panel::logicalRgb(img, showLight);
            SDL_UpdateTexture(tex, nullptr, rgb.data(), img.w * 3);
            std::string title = "FlickBook twin  t=" + std::to_string(nowMs() / 1000) + "s  bat=" +
                                std::to_string(peripherals().battery_v).substr(0, 4) + "V" +
                                (peripherals().wifi_ap ? "  web: http://" + config().web_bind + ":" +
                                                             std::to_string(config().web_port) + "/"
                                                       : "");
            SDL_SetWindowTitle(win, title.c_str());
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
    resetRequested = ctl.reset;
    panel::setFlashPresenter(nullptr);
    if (tex)
        SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
