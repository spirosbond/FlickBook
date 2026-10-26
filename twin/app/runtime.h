// FlickBook digital twin - firmware driver (setup()/loop() scheduling).
#pragma once

#include <cstdint>
#include <string>

void setup(); // provided by FlickBook.ino
void loop();

namespace twin
{
namespace runtime
{

// Run setup() once (idempotent).
void boot();
bool booted();

// One firmware loop() iteration (+ twin hooks, guard checks). In
// deterministic mode the virtual clock advances by config().loop_tick_ms.
void step();

// Run loop() until virtual time has advanced by at least `ms`.
void runFor(uint64_t ms);

struct IdleResult
{
    bool idle = false;
    uint64_t elapsed_ms = 0;
};
// Run until the touch queue is drained and nothing happened (refresh, serial
// output, touch read) for `quiet_ms`, or until `max_ms` elapsed.
IdleResult runUntilIdle(uint64_t quiet_ms = 400, uint64_t max_ms = 60000);

uint64_t loopCount();

} // namespace runtime
} // namespace twin
