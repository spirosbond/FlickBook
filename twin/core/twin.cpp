#include "twin.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <thread>

namespace twin
{

Config &config()
{
    static Config c;
    return c;
}

// ---------------------------------------------------------------- clock ---
namespace
{
uint64_t g_virtualUs = 0;
std::chrono::steady_clock::time_point g_wallStart = std::chrono::steady_clock::now();
uint64_t g_lastActivityMs = 0;

uint64_t wallUs()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - g_wallStart)
        .count();
}
} // namespace

uint64_t nowUs()
{
    if (config().realtime)
        return wallUs();
    return g_virtualUs;
}

uint64_t nowMs() { return nowUs() / 1000; }

void advanceUs(uint64_t us)
{
    if (config().realtime)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
        return;
    }
    g_virtualUs += us;
}

void advanceMs(uint64_t ms) { advanceUs(ms * 1000); }

// ---------------------------------------------------------------- serial --
namespace
{
std::string g_serial;
FILE *g_serialFile = nullptr;
bool g_serialFileTried = false;
} // namespace

void serialWrite(const uint8_t *data, size_t len)
{
    if (!len)
        return;
    g_serial.append(reinterpret_cast<const char *>(data), len);
    if (config().echo_serial)
    {
        fwrite(data, 1, len, stderr);
    }
    if (!g_serialFileTried)
    {
        g_serialFileTried = true;
        if (!config().serial_log_path.empty())
            g_serialFile = fopen(config().serial_log_path.c_str(), "wb");
    }
    if (g_serialFile)
    {
        fwrite(data, 1, len, g_serialFile);
        fflush(g_serialFile);
    }
    noteActivity();
}

const std::string &serialLog() { return g_serial; }

void serialLogMark(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::string line = std::string("[twin] ") + buf + "\n";
    serialWrite(reinterpret_cast<const uint8_t *>(line.data()), line.size());
}

// ----------------------------------------------------------------- touch --
namespace
{
std::deque<TouchSample> g_touch;
} // namespace

void touchEnqueue(const TouchSample &s)
{
    // Keep the queue time-ordered.
    auto it = std::upper_bound(g_touch.begin(), g_touch.end(), s.t,
                               [](uint64_t t, const TouchSample &o) { return t < o.t; });
    g_touch.insert(it, s);
}

bool touchAvailable()
{
    return !g_touch.empty() && g_touch.front().t <= nowMs();
}

uint8_t touchRead(uint16_t *x, uint16_t *y)
{
    // Mirrors Touch::getData(): zero the arrays, then return the controller's
    // latest report. Reports that became due while the firmware was busy are
    // coalesced (the real controller only exposes its current state), but
    // presses and releases are never merged so no gesture is lost.
    x[0] = x[1] = y[0] = y[1] = 0;
    if (!touchAvailable())
        return 0;
    const uint64_t now = nowMs();
    TouchSample s = g_touch.front();
    g_touch.pop_front();
    while (s.fingers != 0 && !g_touch.empty() && g_touch.front().t <= now && g_touch.front().fingers != 0)
    {
        s = g_touch.front();
        g_touch.pop_front();
    }
    for (int i = 0; i < s.fingers && i < 2; ++i)
    {
        x[i] = s.x[i];
        y[i] = s.y[i];
    }
    noteActivity();
    return s.fingers;
}

bool touchPending() { return !g_touch.empty(); }

uint64_t touchLastDueTime() { return g_touch.empty() ? 0 : g_touch.back().t; }

void touchClear() { g_touch.clear(); }

namespace
{
TouchSample sample(uint64_t t, uint8_t n, int x, int y)
{
    TouchSample s;
    s.t = t;
    s.fingers = n;
    s.x[0] = (uint16_t)std::max(0, x);
    s.y[0] = (uint16_t)std::max(0, y);
    return s;
}

uint64_t gestureStart()
{
    // Start after anything already queued so gestures never interleave.
    uint64_t t = nowMs();
    uint64_t last = touchLastDueTime();
    if (last + kTouchReportIntervalMs > t && touchPending())
        t = last + kTouchReportIntervalMs;
    return t;
}
} // namespace

void gestureTap(int x, int y, uint32_t holdMs)
{
    uint64_t t = gestureStart();
    uint32_t elapsed = 0;
    do
    {
        touchEnqueue(sample(t + elapsed, 1, x, y));
        elapsed += kTouchReportIntervalMs;
    } while (elapsed < holdMs);
    touchEnqueue(sample(t + elapsed, 0, 0, 0));
}

void gestureLongPress(int x, int y, uint32_t holdMs) { gestureTap(x, y, holdMs); }

void gestureSwipe(int x0, int y0, int x1, int y1, uint32_t durationMs)
{
    uint64_t t = gestureStart();
    uint32_t steps = std::max<uint32_t>(1, durationMs / kTouchReportIntervalMs);
    for (uint32_t i = 0; i <= steps; ++i)
    {
        int x = x0 + (int)((int64_t)(x1 - x0) * i / steps);
        int y = y0 + (int)((int64_t)(y1 - y0) * i / steps);
        touchEnqueue(sample(t + (uint64_t)i * kTouchReportIntervalMs, 1, x, y));
    }
    touchEnqueue(sample(t + (uint64_t)(steps + 1) * kTouchReportIntervalMs, 0, 0, 0));
}

// ------------------------------------------------------------- peripherals --
Peripherals &peripherals()
{
    static Peripherals p;
    return p;
}

// ------------------------------------------------------- refresh events ---
namespace
{
std::vector<RefreshEvent> g_events;
} // namespace

void recordRefresh(const RefreshEvent &e)
{
    RefreshEvent c = e;
    c.seq = g_events.size() + 1;
    g_events.push_back(c);
    noteActivity();
}

const std::vector<RefreshEvent> &refreshEvents() { return g_events; }

uint64_t lastActivityMs() { return g_lastActivityMs; }

void noteActivity() { g_lastActivityMs = nowMs(); }

// ------------------------------------------------------------------- RNG ---
namespace
{
uint64_t g_rng = 0x9E3779B97F4A7C15ull;
bool g_rngSeeded = false;
} // namespace

void randomSeed(uint32_t seed)
{
    g_rng = 0x9E3779B97F4A7C15ull ^ ((uint64_t)seed * 0xD1B54A32D192ED03ull);
    if (!g_rng)
        g_rng = 1;
    g_rngSeeded = true;
}

uint32_t randomU32()
{
    if (!g_rngSeeded)
        randomSeed(config().seed);
    // xorshift64*
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return (uint32_t)((g_rng * 2685821657736338717ull) >> 32);
}

// ------------------------------------------------------------------ misc ---
namespace
{
std::vector<std::string> g_warnings;
std::vector<Hook> g_hooks;
} // namespace

void warn(const std::string &msg)
{
    g_warnings.push_back(msg);
    serialLogMark("WARNING: %s", msg.c_str());
}

const std::vector<std::string> &warnings() { return g_warnings; }

void addLoopHook(Hook h) { g_hooks.push_back(std::move(h)); }

void runLoopHooks()
{
    for (auto &h : g_hooks)
        h();
}

} // namespace twin
