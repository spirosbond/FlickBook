#include "runtime.h"

#include <chrono>
#include <thread>

#include "Inkplate.h"
#include "twin.h"

extern Inkplate display; // defined in ebook_reader.ino

namespace twin
{
namespace runtime
{
namespace
{
bool g_booted = false;
uint64_t g_loops = 0;
} // namespace

bool booted() { return g_booted; }
uint64_t loopCount() { return g_loops; }

void boot()
{
    if (g_booted)
        return;
    g_booted = true;
    serialLogMark("boot: calling setup()");
    setup();
    display.twinCheckGuards();
    serialLogMark("boot: setup() returned at t=%llu ms", (unsigned long long)nowMs());
}

void step()
{
    if (!g_booted)
        boot();
    loop();
    runLoopHooks();
    display.twinCheckGuards();
    ++g_loops;
    if (config().realtime)
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max<uint32_t>(1, config().loop_tick_ms / 2)));
    else
        advanceMs(config().loop_tick_ms);
}

void runFor(uint64_t ms)
{
    const uint64_t target = nowMs() + ms;
    do
        step();
    while (nowMs() < target);
}

IdleResult runUntilIdle(uint64_t quiet_ms, uint64_t max_ms)
{
    IdleResult r;
    const uint64_t start = nowMs();
    const uint64_t deadline = start + max_ms;
    noteActivity(); // the command itself counts as activity
    while (true)
    {
        step();
        const uint64_t now = nowMs();
        if (!touchPending() && now >= lastActivityMs() + quiet_ms)
        {
            r.idle = true;
            break;
        }
        if (now >= deadline)
            break;
    }
    r.elapsed_ms = nowMs() - start;
    return r;
}

} // namespace runtime
} // namespace twin
