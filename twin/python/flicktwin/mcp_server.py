"""Minimal, dependency-free MCP (Model Context Protocol) server exposing the twin as tools.

Run:  python -m flicktwin mcp [--gray] [-- extra flicktwin args]

Configure in an MCP client (example):
    {"mcpServers": {"flickbook-twin": {"command": "python3",
        "args": ["-m", "flicktwin", "mcp"], "env": {"PYTHONPATH": "<repo>/twin/python"}}}}

Transport: JSON-RPC 2.0, one message per line on stdin/stdout.
"""
import json
import subprocess
import sys
from typing import Any, Dict, List, Optional

from .client import TWIN_DIR, Twin

PROTOCOL_VERSION = "2024-11-05"


def _schema(props: Dict[str, Any], required: Optional[List[str]] = None):
    return {"type": "object", "properties": props, "required": required or []}


INT = {"type": "integer"}
TOOLS = [
    ("tap", "Tap the touchscreen at logical portrait coordinates (758x1024) and run until idle.",
     _schema({"x": INT, "y": INT, "hold_ms": INT}, ["x", "y"])),
    ("long_press", "Press and hold (default 1000 ms) at x,y and run until idle.",
     _schema({"x": INT, "y": INT, "hold_ms": INT}, ["x", "y"])),
    ("swipe", "Swipe. Either direction (finger movement: left/right/up/down) or x0,y0,x1,y1.",
     _schema({"direction": {"type": "string", "enum": ["left", "right", "up", "down"]}, "x0": INT, "y0": INT,
              "x1": INT, "y1": INT, "duration_ms": INT})),
    ("advance_time", "Run the firmware loop for ms milliseconds of virtual time.", _schema({"ms": INT}, ["ms"])),
    ("run_until_idle", "Run until no refresh/serial/touch activity for quiet_ms (cap max_ms).",
     _schema({"quiet_ms": INT, "max_ms": INT})),
    ("screenshot", "Return the e-paper panel as a PNG image (source=panel|framebuffer, frontlight overlay optional).",
     _schema({"source": {"type": "string", "enum": ["panel", "framebuffer"]}, "frontlight": {"type": "boolean"},
              "path": {"type": "string"}})),
    ("get_framebuffer_hash", "Hash of the panel (or framebuffer) pixels for cheap change detection.",
     _schema({"source": {"type": "string", "enum": ["panel", "framebuffer"]}})),
    ("get_pixels", "Gray values (0=black..255=white) of a small region of the panel.",
     _schema({"x": INT, "y": INT, "w": INT, "h": INT}, ["x", "y", "w", "h"])),
    ("get_serial_log", "Firmware Serial output (pass since=<next> from a previous call for increments).",
     _schema({"since": INT})),
    ("get_refresh_events", "E-paper refresh events (full/partial/promoted/ignored) with changed-pixel bbox.",
     _schema({"since": INT})),
    ("get_state", "Twin state: virtual time, screen size, battery, frontlight, wifi, warnings count, etc.",
     _schema({})),
    ("get_warnings", "Problems the twin detected (e.g. framebuffer overruns, PSRAM exhaustion).", _schema({})),
    ("set_battery", "Set the battery voltage returned by readBattery().",
     _schema({"volts": {"type": "number"}}, ["volts"])),
    ("reset", "Restart the firmware with a fresh copy of the SD card.", _schema({})),
    ("load_sdcard", "Restart the firmware using a copy of the given host directory as SD card.",
     _schema({"dir": {"type": "string"}}, ["dir"])),
    ("rebuild", "Rebuild the twin from the current firmware sources (cmake --build) and restart it. "
                "Returns compiler output on failure.", _schema({})),
]


class Server:
    def __init__(self, grayscale=False, args=None):
        self.grayscale = grayscale
        self.args = args or []
        self.twin: Optional[Twin] = None

    def ensure_twin(self) -> Twin:
        if self.twin is None or self.twin.proc.poll() is not None:
            self.twin = Twin(grayscale=self.grayscale, args=self.args)
        return self.twin

    def rebuild(self):
        build = TWIN_DIR / "build"
        if self.twin:
            self.twin.close()
            self.twin = None
        steps = []
        if not (build / "CMakeCache.txt").exists():
            steps.append(["cmake", "-S", str(TWIN_DIR), "-B", str(build)])
        steps.append(["cmake", "--build", str(build), "-j"])
        out = ""
        for cmd in steps:
            p = subprocess.run(cmd, capture_output=True, text=True)
            out += p.stdout[-4000:] + p.stderr[-8000:]
            if p.returncode != 0:
                return False, out
        self.ensure_twin()
        return True, out[-2000:]

    def call_tool(self, name: str, args: Dict[str, Any]):
        if name == "rebuild":
            ok, out = self.rebuild()
            return [{"type": "text", "text": ("build ok\n" if ok else "BUILD FAILED\n") + out}], not ok
        t = self.ensure_twin()
        if name == "screenshot" and "path" not in args:
            res = t.call("screenshot", **args)
            png = res.pop("png_base64")
            return [{"type": "image", "data": png, "mimeType": "image/png"},
                    {"type": "text", "text": json.dumps(res)}], False
        res = t.call(name, **args)
        return [{"type": "text", "text": json.dumps(res)}], False

    def handle(self, msg: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        method, mid = msg.get("method"), msg.get("id")
        if mid is None:
            return None  # notification
        try:
            if method == "initialize":
                result = {"protocolVersion": PROTOCOL_VERSION, "capabilities": {"tools": {}},
                          "serverInfo": {"name": "flickbook-twin", "version": "0.1.0"}}
            elif method == "ping":
                result = {}
            elif method == "tools/list":
                result = {"tools": [{"name": n, "description": d, "inputSchema": s} for n, d, s in TOOLS]}
            elif method == "tools/call":
                p = msg.get("params", {})
                try:
                    content, is_error = self.call_tool(p["name"], p.get("arguments") or {})
                except Exception as e:  # report tool failures as tool errors, not protocol errors
                    content, is_error = [{"type": "text", "text": "error: %s" % e}], True
                result = {"content": content, "isError": is_error}
            else:
                return {"jsonrpc": "2.0", "id": mid, "error": {"code": -32601, "message": "unknown method"}}
            return {"jsonrpc": "2.0", "id": mid, "result": result}
        except Exception as e:
            return {"jsonrpc": "2.0", "id": mid, "error": {"code": -32603, "message": str(e)}}

    def serve(self, inp=sys.stdin, out=sys.stdout):
        for line in inp:
            line = line.strip()
            if not line:
                continue
            try:
                msg = json.loads(line)
            except json.JSONDecodeError:
                out.write(json.dumps({"jsonrpc": "2.0", "id": None,
                                      "error": {"code": -32700, "message": "parse error"}}) + "\n")
                out.flush()
                continue
            res = self.handle(msg)
            if res is not None:
                out.write(json.dumps(res) + "\n")
                out.flush()
        if self.twin:
            self.twin.close()
