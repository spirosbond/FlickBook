// FlickBook digital twin - shared runtime state used by the hardware shims
// (Arduino core, Inkplate, SdFat, WebServer) and by the twin front-ends
// (headless runner, SDL window, agent API).
//
// Everything here is single-threaded: the firmware's setup()/loop() and the
// twin's command handling all run on the main thread, which keeps runs
// deterministic.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace twin
{

struct Config
{
    // Clock
    bool realtime = false;       // true: millis() follows wall time, delay() sleeps
    uint32_t loop_tick_ms = 10;  // virtual time added per loop() iteration (deterministic mode)
    bool simulate_refresh_time = true;
    uint32_t full_refresh_ms = 1100;   // ~1 s full refresh on Inkplate 6 FLICK (1-bit)
    uint32_t full_refresh_3bit_ms = 1700;
    uint32_t partial_refresh_ms = 300;

    // Panel model
    bool ghosting = true; // partial updates leave faint residue of pixels that went black -> white
    bool refresh_flash = true; // viewers animate the driver's full-refresh clear sequence

    // Serial
    bool echo_serial = true;
    std::string serial_log_path;

    // SD card
    std::string sd_root;          // host directory mapped to the SD card root
    bool sd_present = true;       // false: sdCardInit() fails (card missing)
    bool sd_read_errors = false;  // true: file reads fail after open
    uint64_t sd_capacity_bytes = 0; // 0 = unlimited, otherwise writes fail when exceeded

    // Memory model
    size_t psram_bytes = 4u * 1024u * 1024u; // single ps_malloc() larger than this fails
    uint32_t free_heap_bytes = 180u * 1024u; // reported by ESP.getFreeHeap()

    // Touch
    bool touch_init_ok = true;

    // RNG
    uint32_t seed = 1;

    // Networking: the firmware's WebServer(80) is mapped to this host port.
    int web_port = 8080;
    std::string web_bind = "127.0.0.1";
};

Config &config();

// ---------------------------------------------------------------- clock ---
uint64_t nowMs();
uint64_t nowUs();
// Advance virtual time (deterministic mode) or sleep (realtime mode).
void advanceMs(uint64_t ms);
void advanceUs(uint64_t us);

// ---------------------------------------------------------------- serial --
void serialWrite(const uint8_t *data, size_t len);
const std::string &serialLog();
void serialLogMark(const char *fmt, ...); // twin-originated log lines ("[twin] ...")

// ----------------------------------------------------------------- touch --
struct TouchSample
{
    uint64_t t = 0; // virtual time at which the controller reports this sample
    uint8_t fingers = 0;
    uint16_t x[2] = {0, 0};
    uint16_t y[2] = {0, 0};
};
// Samples are in *logical* (rotated) screen coordinates, i.e. exactly what
// Touch::getData() returns on the device for the current rotation.
void touchEnqueue(const TouchSample &s);
bool touchAvailable();
uint8_t touchRead(uint16_t *x, uint16_t *y);
bool touchPending();      // anything still queued (due or in the future)
uint64_t touchLastDueTime(); // time of the last queued sample (0 if none)
void touchClear();

// Gesture helpers (schedule samples starting at the current virtual time).
// Report interval mimics the Cypress controller's active scan rate.
constexpr uint32_t kTouchReportIntervalMs = 15;
void gestureTap(int x, int y, uint32_t holdMs = 60);
void gestureLongPress(int x, int y, uint32_t holdMs = 1000);
void gestureSwipe(int x0, int y0, int x1, int y1, uint32_t durationMs = 150);

// ------------------------------------------------------------- peripherals --
struct Peripherals
{
    double battery_v = 4.05;
    bool frontlight_on = false;
    uint8_t frontlight_level = 0; // 0..63
    bool wifi_ap = false;
    std::string wifi_ssid;
};
Peripherals &peripherals();

// ------------------------------------------------------- refresh events ---
struct RefreshEvent
{
    uint64_t seq = 0;
    uint64_t t = 0;
    std::string kind; // "full" | "partial" | "partial_promoted_full" | "partial_ignored_3bit"
    uint8_t mode = 0; // 0 = 1-bit, 1 = 3-bit
    uint32_t changed = 0; // pixels that changed on the panel
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1; // bounding box of changes (logical coords, inclusive)
};
void recordRefresh(const RefreshEvent &e);
const std::vector<RefreshEvent> &refreshEvents();
uint64_t lastActivityMs(); // last refresh / serial output / touch consumption
void noteActivity();

// ------------------------------------------------------------------- RNG ---
uint32_t randomU32();
void randomSeed(uint32_t seed);

// ------------------------------------------------------------------ misc ---
// Called by shims when the firmware does something the twin wants agents to
// know about (e.g. buffer overruns). Collected in warnings() and logged.
void warn(const std::string &msg);
const std::vector<std::string> &warnings();

// Firmware entry points (provided by the compiled sketch).
using Hook = std::function<void()>;
void addLoopHook(Hook h); // run once per loop iteration (e.g. WebServer polling helpers)
void runLoopHooks();

} // namespace twin
