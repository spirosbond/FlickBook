"""Agent command interface: determinism, time control, reset, SD loading, refresh model."""
import shutil
from pathlib import Path

import pytest
from conftest import TWIN_DIR, free_port

from flicktwin import Twin, TwinError, read_png


def _session(t):
    t.tap(630, 250)
    t.swipe("left")
    t.swipe("left")
    return t.framebuffer_hash(), t.get_state()["time_ms"], [e["kind"] for e in t.refresh_events()]


def test_runs_are_deterministic():
    with Twin() as a, Twin() as b:
        assert _session(a) == _session(b)


def test_ready_state(twin):
    s = twin.state
    assert s["booted"] and not s["realtime"]
    assert (s["width"], s["height"]) == (758, 1024)
    assert s["rotation"] == 1


def test_advance_time_is_virtual(twin):
    t0 = twin.get_state()["time_ms"]
    twin.advance_time(60_000)  # a minute of firmware time, instantly
    assert twin.get_state()["time_ms"] >= t0 + 60_000


def test_screenshot_inline_png_matches_hash(twin, tmp_path):
    img = twin.screenshot()
    assert (img.width, img.height) == (758, 1024)
    path = twin.screenshot(tmp_path / "s.png")
    assert read_png(path) == img
    assert set(img.pixels) <= {0, 255}  # 1-bit mode renders pure black/white


def test_get_pixels_reads_header_bar(twin):
    rows = twin.call("get_pixels", x=0, y=0, w=4, h=4)["rows"]
    assert len(rows) == 4 and all(len(r) == 4 for r in rows)


def test_unknown_command_is_an_error(twin):
    with pytest.raises(TwinError):
        twin.call("does_not_exist")


def test_partial_updates_and_full_refresh_promotion(twin):
    twin.tap(630, 250)
    since = twin.refresh_events()[-1]["seq"] + 1
    for _ in range(10):
        twin.swipe("left")
    kinds = [e["kind"] for e in twin.refresh_events(since)]
    assert "partial" in kinds
    # setFullUpdateThreshold(PARTIAL_UPDATE_LIMIT_N) forces a periodic full refresh.
    assert "partial_promoted_full" in kinds or "full" in kinds


def test_reset_restarts_with_fresh_sd(twin):
    sd = Path(twin.get_state()["sd_root"])
    (sd / "books" / "scratch.txt").write_text("x")
    twin.tap(630, 250)
    twin.reset()
    s = twin.get_state()
    assert s["time_ms"] < 5000
    assert not (Path(s["sd_root"]) / "books" / "scratch.txt").exists()
    assert "Total: 3" in twin.serial_log()


def test_load_sdcard(twin, tmp_path):
    card = tmp_path / "card"
    shutil.copytree(TWIN_DIR.parent / "sdcard", card)
    (card / "books").mkdir()
    shutil.copy(TWIN_DIR / "fixtures" / "books" / "twin_test_book.epub", card / "books")
    twin.load_sdcard(card)
    assert "Total: 1" in twin.serial_log()
    assert not (card / "library").exists(), "the template directory must not be modified"


def test_battery_and_frontlight_state(twin):
    twin.set_battery(3.4)
    s = twin.get_state()
    assert s["battery_v"] == pytest.approx(3.4)
    assert s["frontlight_on"] and s["frontlight_level"] == 5  # settings.json backlight=5


def test_psram_cap_is_reported():
    # 32 KiB lets the icons load but the gallery's 160x240 cover buffers
    # (ImageScaler grayscale buffer) must be refused.
    with Twin(args=["--psram", "32768"]) as t:
        t.tap(330, 250)  # gallery book -> image buffers in PSRAM
        assert "larger than PSRAM" in t.serial_log()
        assert t.call("ping")["ok"]


def test_firmware_crash_is_reported():
    # With 4 KiB the Inkplate PNG decoder dereferences a failed ps_malloc
    # during boot - the twin must surface that as a crash, not hang.
    with pytest.raises(TwinError) as exc:
        with Twin(args=["--psram", "4096"]) as t:
            t.call("ping")
    assert "FIRMWARE CRASH" in str(exc.value)
