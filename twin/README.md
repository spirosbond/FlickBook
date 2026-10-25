# FlickBook digital twin

A host (Linux/macOS) build of the **unmodified FlickBook firmware** with a
simulated Inkplate 6 FLICK: e-paper panel, touchscreen, frontlight, battery,
SD card, PSRAM, WiFi soft-AP and the upload web server. You can use it
interactively, headless, from scripts, or from AI agents (JSON API, Python
client, MCP server).

```
firmware (*.cpp, ebook_reader.ino)   <- compiled as-is
        │ Arduino / Inkplate / SdFat / WiFi / WebServer APIs
twin/shim + twin/core                 <- host implementations, virtual clock, panel model
        │
twin/app: headless | --stdio | --http (browser viewer) | --window (SDL2)
        │
twin/python/flicktwin: client, scenario runner, golden images, MCP server
```

> **Why not a full emulator?** Existing ESP32 emulators (Espressif QEMU, Wokwi)
> boot real binaries, but none model the Inkplate's e-paper controller, its
> TPS65186 PMIC, the FLICK touch controller or the frontlight. A HAL-level twin
> runs the real application code in milliseconds and with full determinism. That
> is what an edit, build and test loop needs. See the accuracy table in
> [AGENTS.md](AGENTS.md#what-the-twin-models-and-what-it-does-not).

## Build

Requirements: CMake ≥ 3.16, a C++17 compiler, Python 3.8+. SDL2 is optional;
install `libsdl2-dev` to get the interactive window. Third-party libraries are
fetched at configure time and pinned by commit or tag:

- ArduinoJson
- tinyxml2
- unzipLIB
- the Inkplate Arduino library (its Adafruit_GFX, image decoders and
  shapes)
- stb

```sh
cmake -S twin -B twin/build -DCMAKE_BUILD_TYPE=Release
cmake --build twin/build -j
```

This produces two binaries:

- `twin/build/flicktwin`: 1-bit, `GRAYSCALE=0`
- `twin/build/flicktwin_gray`: 3-bit, `GRAYSCALE=1`. Disable it with
  `-DTWIN_BUILD_GRAYSCALE=OFF`.

To build a firmware checkout somewhere else, pass `-DFLICKBOOK_DIR=/path/to/FlickBook`.
To move the twin into its own repository, add FlickBook as a git submodule
and point `FLICKBOOK_DIR` at it.

### Host-only JPEGDEC workaround

`libs/jpeg_inl.h` indexes far out of bounds when skipping progressive JPEG
blocks (`iMCU & 0xffffff`). The ESP32 tolerates the stray write; 64-bit Linux
crashes. CMake patches a copy in `build/jpegdec/` only; `libs/` and device
builds are unchanged.

## Run

```sh
# Headless: boot, wait until idle, write a PNG of the panel
twin/build/flicktwin --screenshot boot.png

# Interactive window (real-time clock)
twin/build/flicktwin --window

# Browser viewer + HTTP agent API (good for remote machines)
twin/build/flicktwin --http 8000      # open http://127.0.0.1:8000/

# Agent API: JSON lines on stdin/stdout
echo '{"cmd":"tap","x":630,"y":250}' | twin/build/flicktwin --stdio --quiet

# Scripted: run a file of JSON-line commands, then screenshot
twin/build/flicktwin --script steps.jsonl --screenshot out.png
```

Window keys:

| Key | Action |
|-----|--------|
| mouse | click = tap, drag = swipe, hold = long press |
| ← → ↑ ↓ | swipe left, right, up or down |
| `L` | toggle the frontlight overlay |
| `F` | show the raw framebuffer instead of the panel |
| `[` `]` | battery voltage −/+ 0.1 V |
| `S` | save a screenshot |
| `R` | reset (fresh SD card) |
| `Esc`/`Q` | quit |

Useful options (run `flicktwin --help` to see them all):

| Option | Meaning |
|--------|---------|
| `--sd DIR` / `--sd-template DIR` | use DIR as the SD card (persistent), or copy it into a fresh temp card. The default template is the firmware's `sdcard/` folder plus `twin/fixtures/books/*.epub` |
| `--realtime` / `--deterministic` | clock mode. The default is deterministic; `--window` uses real time |
| `--seed N` | seed for `random()` |
| `--web-port N` | host port that the firmware's `WebServer(80)` binds to. The default is 8080 |
| `--battery V` | battery voltage reported by `readBattery()` |
| `--no-sd-card`, `--sd-read-errors`, `--sd-capacity BYTES` | SD fault injection |
| `--psram BYTES` | cap on a single PSRAM allocation; larger `ps_malloc` calls return NULL |
| `--no-touch` | `touchscreen.init()` fails |
| `--no-ghosting`, `--no-refresh-time` | panel model switches |
| `--no-refresh-flash` | no black/white flash on full refreshes in the window and browser viewer |

The firmware's upload web server works for real. In Settings → tab 3, toggle
"Web server", then open `http://127.0.0.1:8080/`. While WiFi is on, the twin
keeps the firmware loop running.

## Test

```sh
python3 -m pip install pytest
cd twin && python3 -m pytest -q tests                     # both display modes
python3 twin/tools/check_api.py                           # API drift check
PYTHONPATH=twin/python python3 -m flicktwin run twin/tests/scenarios/*.json
```

`FLICKTWIN_UPDATE_GOLDENS=1` regenerates the golden images in
`tests/golden/{1bit,3bit}/`. Review the PNGs before you commit them.
Failing runs leave the actual image, a diff image and the serial log in
`twin/artifacts/<scenario>-<mode>/`.

CI (`.github/workflows/twin.yml`) runs these steps on every push:

- checks for API drift
- builds both modes
- runs all tests
- uploads the artifacts

## Layout

| Path | Contents |
|------|----------|
| `shim/` | Host versions of `Arduino.h`, `WString`, `Print`, `Inkplate.h`, `SdFat.h`, `WiFi.h`, `WebServer.h`, `DNSServer.h`, … |
| `overlay/` | Files laid over the staged Inkplate library (board size, SdFat forwarder) |
| `core/` | Virtual clock, serial log, touch queue, Inkplate/panel model, SD card, Arduino runtime, HTTP server |
| `app/` | `setup()`/`loop()` runtime, agent command handler, HTTP API, SDL window, CLI |
| `viewer/` | Browser viewer served by `--http` |
| `python/flicktwin/` | Python client, scenario runner, PNG utilities, MCP server |
| `tests/` | pytest suites, JSON scenarios, golden images |
| `fixtures/books/` | Small generated test EPUBs (`tools/make_test_epub.py`) |
| `tools/` | Fixture generator, API drift check and its manifest |
