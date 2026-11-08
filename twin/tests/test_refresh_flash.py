"""Viewer-only full-refresh flash: the driver's clear sequence is shown by the SDL window and
the browser viewer, without changing the panel model, screenshots or the agent API."""
import json
import os
import re
import subprocess
import time
import urllib.error
import urllib.request

import pytest
from conftest import free_port

from flicktwin import default_binary

LEVELS = [255, 0, 255, 0, 255, -1]  # white, black, white, black, white, new image


def _req(url, data=None):
    headers = {"Content-Type": "application/json"} if data else {}
    with urllib.request.urlopen(urllib.request.Request(url, data=data, headers=headers), timeout=30) as r:
        return r.read()


def _start(args, env=None):
    port = free_port()
    proc = subprocess.Popen([default_binary(False), "--http", str(port), "--quiet"] + args,
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, env=env)
    base = "http://127.0.0.1:%d" % port
    for _ in range(100):
        if proc.poll() is not None:
            return proc, None
        try:
            _req(base + "/api/state")
            return proc, base
        except (urllib.error.URLError, ConnectionError):
            time.sleep(0.1)
    proc.kill()
    raise AssertionError("twin did not start")


def _stop(proc):
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def _served_flash(args):
    proc, base = _start(args)
    try:
        page = _req(base + "/").decode()
    finally:
        _stop(proc)
    m = re.search(r"const FLASH = (.*?);\n", page)
    assert m, "viewer has no FLASH constant"
    return json.loads(m.group(1))


def test_viewer_gets_driver_phases():
    flash = _served_flash([])
    for mode, total in (("1bit", 1100), ("3bit", 1700)):
        phases = flash[mode]
        assert [lvl for lvl, _ in phases] == LEVELS
        assert sum(ms for _, ms in phases) == total
        assert all(ms > 0 for _, ms in phases)


def test_no_refresh_flash_disables_viewer_flash():
    assert _served_flash(["--no-refresh-flash"]) is None


def _timed_full_refresh(extra):
    env = dict(os.environ, SDL_VIDEODRIVER="dummy")
    proc, base = _start(["--window"] + extra, env)
    if base is None:
        err = proc.stderr.read()
        if "no SDL2 window support" in err:
            pytest.skip("twin built without SDL2")
        raise AssertionError("window mode failed to start:\n" + err)
    try:
        cmd = lambda **c: json.loads(_req(base + "/api/cmd", json.dumps(c).encode()))
        since = json.loads(_req(base + "/api/state"))["refresh_count"] + 1
        t0 = time.monotonic()
        assert cmd(cmd="tap", x=330, y=250)["ok"]  # open Twin Gallery: full refresh
        elapsed = time.monotonic() - t0
        kinds = [e["kind"] for e in cmd(cmd="get_refresh_events", since=since)["events"]]
        assert proc.poll() is None
        assert cmd(cmd="quit")["ok"]
        proc.wait(timeout=10)
        return kinds.count("full"), elapsed
    finally:
        if proc.poll() is None:
            _stop(proc)


def test_sdl_window_flash_keeps_refresh_time():
    full_on, t_on = _timed_full_refresh([])
    full_off, t_off = _timed_full_refresh(["--no-refresh-flash"])
    assert full_on >= 1 and full_off == full_on
    # The flash replaces the realtime refresh wait instead of adding to it.
    assert abs(t_on - t_off) < 0.25 * full_on, (t_on, t_off)


def test_sdl_window_draws_flash():
    # Without simulated refresh time, the only wall-clock cost of a full
    # refresh is the drawn flash (~1 s of phases before the new image).
    full_on, t_on = _timed_full_refresh(["--no-refresh-time"])
    _, t_off = _timed_full_refresh(["--no-refresh-time", "--no-refresh-flash"])
    assert t_on - t_off > 0.4 * full_on, (t_on, t_off)
