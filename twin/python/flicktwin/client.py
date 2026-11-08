"""Python client for the FlickBook digital twin.

    from flicktwin import Twin
    with Twin() as t:                       # spawns `flicktwin --stdio`
        t.tap(100, 250)                     # open the first book
        t.swipe("left")                     # next page
        t.screenshot("page.png")
        print(t.serial_log())

Every method maps 1:1 to a JSON command of the twin's agent API (see
twin/AGENTS.md). Unknown attributes are forwarded too: ``t.call("get_state")``.
"""
import json
import os
import subprocess
import threading
from pathlib import Path
from typing import Any, Dict, List, Optional

from .png import read_png

TWIN_DIR = Path(__file__).resolve().parents[2]


class TwinError(RuntimeError):
    pass


def default_binary(grayscale: bool = False) -> str:
    env = os.environ.get("FLICKTWIN_GRAY_BIN" if grayscale else "FLICKTWIN_BIN")
    if env:
        return env
    name = "flicktwin_gray" if grayscale else "flicktwin"
    for build in ("build", "build-release", "build-debug"):
        p = TWIN_DIR / build / name
        if p.exists():
            return str(p)
    raise TwinError("flicktwin binary not found; build it (cmake -S twin -B twin/build && cmake --build twin/build) "
                    "or set FLICKTWIN_BIN")


class Twin:
    """A running twin process driven over the stdio JSON-lines protocol."""

    def __init__(self, binary: Optional[str] = None, args: Optional[List[str]] = None, grayscale: bool = False,
                 sd_template: Optional[str] = None, books: Optional[str] = None, fixtures: bool = True,
                 timeout: float = 120.0):
        self.binary = binary or default_binary(grayscale)
        cmd = [self.binary, "--stdio"]
        if sd_template:
            cmd += ["--sd-template", str(sd_template)]
        if books:
            cmd += ["--books", str(books)]
        if not fixtures:
            cmd += ["--no-fixtures"]
        cmd += list(args or [])
        self.timeout = timeout
        self._stderr: List[str] = []
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, bufsize=1)
        self._stderr_thread = threading.Thread(target=self._drain_stderr, daemon=True)
        self._stderr_thread.start()
        self.state = self._wait_ready()

    # ------------------------------------------------------------ plumbing --
    def _drain_stderr(self):
        for line in self.proc.stderr:
            self._stderr.append(line)

    def _readline(self) -> Dict[str, Any]:
        line = self.proc.stdout.readline()
        if not line:
            try:
                self.proc.wait(timeout=5)
                self._stderr_thread.join(timeout=5)
            except subprocess.TimeoutExpired:
                pass
            raise TwinError("twin exited (rc=%s). stderr tail:\n%s" % (self.proc.poll(), "".join(self._stderr[-30:])))
        return json.loads(line)

    def _wait_ready(self) -> Dict[str, Any]:
        msg = self._readline()
        if msg.get("event") != "ready":
            raise TwinError("unexpected first message: %r" % msg)
        return msg["state"]

    def call(self, cmd: str, **kwargs) -> Dict[str, Any]:
        req = dict(kwargs, cmd=cmd)
        self.proc.stdin.write(json.dumps(req) + "\n")
        self.proc.stdin.flush()
        res = self._readline()
        if not res.get("ok"):
            raise TwinError("%s failed: %s" % (cmd, res.get("error")))
        if cmd in ("reset", "load_sdcard"):
            self.state = self._wait_ready()
        return res

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda **kw: self.call(name, **kw)

    def close(self):
        if self.proc.poll() is None:
            try:
                self.proc.stdin.write(json.dumps({"cmd": "quit"}) + "\n")
                self.proc.stdin.flush()
                self.proc.wait(timeout=10)
            except Exception:
                self.proc.kill()
                self.proc.wait()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    @property
    def stderr(self) -> str:
        return "".join(self._stderr)

    # --------------------------------------------------------- convenience --
    def tap(self, x: int, y: int, **kw):
        return self.call("tap", x=x, y=y, **kw)

    def long_press(self, x: int, y: int, hold_ms: int = 1000, **kw):
        return self.call("long_press", x=x, y=y, hold_ms=hold_ms, **kw)

    def swipe(self, direction: Optional[str] = None, *, x0=None, y0=None, x1=None, y1=None, **kw):
        if direction:
            return self.call("swipe", direction=direction, **kw)
        return self.call("swipe", x0=x0, y0=y0, x1=x1, y1=y1, **kw)

    def advance_time(self, ms: int):
        return self.call("advance_time", ms=ms)

    def run_until_idle(self, quiet_ms: int = 400, max_ms: int = 60000):
        return self.call("run_until_idle", quiet_ms=quiet_ms, max_ms=max_ms)

    def screenshot(self, path: Optional[str] = None, **kw):
        """Save a PNG (absolute path returned) or, without a path, return decoded pixels."""
        if path:
            return self.call("screenshot", path=str(Path(path).resolve()), **kw)["path"]
        import base64
        res = self.call("screenshot", **kw)
        return read_png(base64.b64decode(res["png_base64"]))

    def framebuffer_hash(self, source: str = "panel") -> str:
        return self.call("get_framebuffer_hash", source=source)["hash"]

    def serial_log(self, since: int = 0) -> str:
        return self.call("get_serial_log", since=since)["text"]

    def refresh_events(self, since: int = 0) -> List[Dict[str, Any]]:
        return self.call("get_refresh_events", since=since)["events"]

    def warnings(self) -> List[str]:
        return self.call("get_warnings")["warnings"]

    def set_battery(self, volts: float):
        return self.call("set_battery", volts=volts)

    def get_state(self) -> Dict[str, Any]:
        return self.call("get_state")

    def reset(self):
        return self.call("reset")

    def load_sdcard(self, directory: str):
        return self.call("load_sdcard", dir=str(Path(directory).resolve()))
