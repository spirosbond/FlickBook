# AGENTS.md: developing FlickBook with the digital twin

Use the twin to check every firmware change before it reaches a device. It
compiles the real firmware sources (`../*.cpp`, `../ebook_reader.ino`)
against simulated hardware. Runs are **deterministic**: the same commands
always produce the same pixels, serial log and refresh events.

## Workflow

1. **Edit** firmware sources in the repository root, as usual.
2. **Rebuild** the twin: `cmake --build twin/build -j`. Configure it once
   with `cmake -S twin -B twin/build`.
   Compile errors here are compile errors on the device too, except for
   ESP32-only APIs. If you use a *new* hardware API, see
   [API drift](#api-drift).
3. **Run the scenarios**: `cd twin && python3 -m pytest -q tests`. This runs
   the 1-bit and 3-bit builds.
4. **Inspect** the PNGs in `twin/artifacts/<scenario>-<mode>/`:
   - `final.png`
   - `<golden>.png` (actual) and `<golden>.diff.png`
   - `failure.png`
   - `serial.log`
5. **Add or extend a scenario** in `twin/tests/scenarios/` for the behaviour
   you changed. Then generate its goldens:
   `FLICKTWIN_UPDATE_GOLDENS=1 python3 -m pytest -q tests -k <name>`. Look at
   the new PNGs before you commit them. Goldens are the reference for every
   later change.

If an existing golden changes, the change must be intentional. Check the diff
image, then regenerate the golden.

## Driving the twin

### Python (recommended)

```python
import sys; sys.path.insert(0, "twin/python")
from flicktwin import Twin

with Twin() as t:                     # Twin(grayscale=True) for the 3-bit build
    t.tap(630, 250)                   # open the third library book
    for _ in range(3):
        t.swipe("left")               # next page (direction = finger movement)
    t.screenshot("/tmp/page4.png")
    print(t.serial_log()[-2000:])
    print(t.refresh_events()[-3:])
    assert not t.warnings()
```

Every command is also available as `t.call("<cmd>", **args)`. Interaction
commands run the firmware until it is idle, then return. Idle means no
refresh, serial output or touch for 400 ms of virtual time.

### Commands (JSON lines on `--stdio`, or `POST /api/cmd` with `--http`)

| Command | Arguments | Result |
|---------|-----------|--------|
| `tap` | `x, y, [hold_ms=60]` | touch down/up, then run until idle |
| `long_press` | `x, y, [hold_ms=1000]` | |
| `swipe` | `direction` (`left`/`right`/`up`/`down`) **or** `x0,y0,x1,y1`, `[duration_ms]` | |
| `touch` | `samples: [{x,y,down,t}]` | raw touch-controller samples |
| `advance_time` | `ms` | run `loop()` for `ms` of virtual time |
| `run_until_idle` | `[quiet_ms, max_ms]` | |
| `screenshot` | `[path]`, `[source=panel\|framebuffer]`, `[frontlight]` | PNG file, or base64 `png` |
| `get_framebuffer_hash` | `[source]` | `hash` |
| `get_pixels` | `x, y, w, h` | gray values 0 (black) … 255 (white) |
| `get_serial_log` | `[since]` | `log`, `next` |
| `get_refresh_events` | `[since]` | `events: [{seq, t, kind, mode, changed, bbox}]` |
| `get_warnings` | | twin-detected problems |
| `get_state` | | time, screen size, battery, frontlight, WiFi, counters |
| `set_battery` | `volts` | |
| `reset` | | restart the firmware with a fresh SD copy (prints a new `ready` line) |
| `load_sdcard` | `dir` | restart the firmware with a copy of `dir` as the SD card |
| `ping`, `quit` | | |

Coordinates are **logical portrait pixels** (758×1024, origin top left), the
same as the firmware UI code (`UIManager.h`). Refresh-event `kind` is one of:

- `full`: `display()`
- `partial`: `partialUpdate()`
- `partial_promoted_full`: the `setFullUpdateThreshold` count was reached
- `partial_ignored_3bit`: a partial update in 3-bit mode, which the Inkplate library ignores

### UI cheat sheet (default fixtures, 1-bit)

| Target | Coordinates |
|--------|-------------|
| Library books 1 / 2 / 3: Edge Cases / Twin Gallery / Keeper's Almanac | (100,250) / (330,250) / (630,250) |
| Settings gear | (662,30) |
| Settings tabs 1 / 2 / 3 | (100,150) / (200,150) / (300,150) |
| Dark mode toggle (tab 1) | (670,450) |
| Web server toggle (tab 3) | (670,250) |
| Home (in book) | (40,980) |
| Next / previous page | `swipe left` / `swipe right` |

### Scenarios

JSON scenario files in `tests/scenarios/` are lists of `cmd` steps and
`expect` steps:

- `golden`
- `serial_contains`
- `serial_not_contains`
- `refresh`
- `no_warnings`
- `warning_contains`
- `state`
- `hash`

Steps can be scoped to `"scope": "last"` (output of the last command) or to a
display mode (`"modes": ["1bit"]`). The format is documented at the top of
`python/flicktwin/scenario.py`. Run them with pytest or with
`PYTHONPATH=twin/python python3 -m flicktwin run <files>`.

### MCP server

`python3 -m flicktwin mcp` exposes the command set as MCP tools over stdio.
It also adds a `rebuild` tool that recompiles the twin and restarts it.
`screenshot` returns the PNG as image content. Example client configuration:

```json
{
  "mcpServers": {
    "flickbook-twin": {
      "command": "python3",
      "args": ["-m", "flicktwin", "mcp"],
      "env": {"PYTHONPATH": "twin/python"}
    }
  }
}
```

Add `--gray` for the 3-bit build. Arguments after `--` go to the binary, for
example `-- --battery 3.3`.

## Things the twin checks for you

- **Framebuffer overruns**: guard bytes around the Inkplate buffers. Writes
  past the end are reported in `get_warnings` and as a `[twin]` serial line.
  The current firmware triggers one: dark mode's `clearDisplay()` memsets
  `TOT_W*TOT_H/8` bytes into the 1-bit `_partial` buffer, which is 1280 bytes
  too many (5120 in 3-bit mode). The `settings_dark_mode` scenario documents
  this.
- **Crashes**: a segfault or abort prints
  `[twin] FIRMWARE CRASH (Guru Meditation equivalent)` with a backtrace, and
  the Python client raises `TwinError`. Build with `-DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_CXX_FLAGS=-fsanitize=address` to get exact source lines.
- **PSRAM exhaustion**: with `--psram BYTES`, any larger allocation fails and is
  logged. Allocations made through the Inkplate PNG decoder are not
  NULL-checked. A tiny cap crashes it on the device as well.
- **Refresh behaviour**: every `display()`/`partialUpdate()` is recorded with
  the number of changed pixels and their bounding box. Use this to catch
  redundant refreshes (`changed: 0`) or unexpected full refreshes.

## What the twin models, and what it does not

| Area | Modelled | Not modelled or approximate |
|------|----------|----------------------------|
| Firmware logic | All application sources, compiled unmodified, plus the real ArduinoJson, tinyxml2, unzipLIB, JPEGDEC, qrcode and Inkplate graphics/image decoders | ESP32 compiler differences (int sizes, `char` signedness, `-O2` vs. host UB) |
| Display | 1024×758 panel, rotation, 1-bit and 3-bit modes; full and partial refresh semantics; `setFullUpdateThreshold`; optional ghosting after partial updates; approximate refresh durations (1.1 s / 1.7 s / 0.3 s) added to the virtual clock | Real waveforms, temperature dependence, exact gray levels and timing, TPS65186 power sequencing |
| Touch | `touchscreen.init/available/getData`, `tsAvailable/tsGetData`, taps, swipes, long presses, raw samples at a 15 ms report rate | Controller quirks (noise, multi-touch ghosts, report-rate jitter, palm rejection) |
| Frontlight | `setState`, `setBrightness(0..63)` as a warm overlay in screenshots | Light uniformity, PWM behaviour |
| Battery | `readBattery()` returns a settable voltage | Discharge curve, ADC noise |
| SD card | SdFat API on a host directory; missing card, read errors and full-card faults | FAT limits (8.3 names, cluster sizes), SPI timing, card wear |
| Memory | `ps_malloc`/`heap_caps_*` with a per-allocation PSRAM cap; framebuffer guard bytes | Heap fragmentation, internal-RAM limits, stack size and overflow, `ESP.getFreeHeap()` realism |
| Time | Virtual clock (deterministic) or wall clock (real time) for `millis`/`delay` | CPU speed: firmware computation takes no virtual time |
| WiFi / web | `softAP` (no-op that reports 192.168.4.1), `DNSServer` stub, `WebServer` backed by a real localhost HTTP server (uploads, args, downloads) | Radio behaviour, captive-portal DNS, throughput, connection drops |
| Boot / power | `setup()` then `loop()` | Deep sleep, wake-up sources, brown-out, OTA, partition table, real boot ROM |

For boot-level checks of the real ESP32 binary, a future step is to run the
Arduino-built firmware in Espressif's QEMU fork (`qemu-system-xtensa -machine
esp32`). That only proves the image boots and logs over UART; there is no
e-paper there.

## API drift

`tools/check_api.py` scans the firmware for hardware-facing calls and external
includes:

- receivers: `display->…`, `touchscreen.…`, `frontlight.…`, `Serial.…`,
  `WiFi.…`, `server.…`, `dnsServer.…`, `SD.…`, `ESP.…`
- external includes

It compares them with the reviewed list in `tools/api_manifest.json`, and it
runs in CI and in pytest. When the firmware starts to use something new:

1. Implement or verify the behaviour in `shim/` or `core/`. Compiling is not
   enough; make the behaviour match the device.
2. Add a scenario that exercises it.
3. Run `python3 twin/tools/check_api.py --update` and commit the manifest
   diff.

## Firmware-side notes

- `definitions.h` wraps `#define GRAYSCALE` in `#ifndef GRAYSCALE`, so the
  twin's build system can select the mode. Device builds are unchanged.
- The twin defines `FLICKBOOK_TWIN`. Use `#ifdef FLICKBOOK_TWIN` only as a
  last resort; nothing in the firmware needs it today.
- The Inkplate library's JPEG decoder (TJpgDec) only supports YCbCr JPEGs.
  Single-channel grayscale JPEGs fail with "Failed to render image!" on the
  device and in the twin. The fixture generator therefore writes YCbCr files.
