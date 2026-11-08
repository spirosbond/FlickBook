"""Declarative scenario runner.

A scenario is a JSON file::

    {
      "name": "open_book_and_turn_pages",
      "description": "boot, open the first book, swipe left 3 times, compare against golden",
      "twin": {"grayscale": false, "args": [], "fixtures": true, "sd_template": null},
      "steps": [
        {"cmd": "tap", "x": 100, "y": 250},
        {"cmd": "swipe", "direction": "left", "repeat": 3},
        {"expect": "golden", "name": "edge_cases_page4", "tolerance": 0.001},
        {"expect": "serial_contains", "text": "Touch Release"},
        {"expect": "refresh", "kind": "partial", "min": 1, "scope": "last"},
        {"expect": "no_warnings"}
      ]
    }

Steps with "cmd" are sent to the twin (any agent command; "repeat" repeats it).
Steps with "expect" are assertions:

  golden               compare the panel with tests/golden/<mode>/<name>.png
                       ("tolerance" = max fraction of differing pixels, "threshold" = gray delta)
  serial_contains      "text" appears in the serial log
  serial_not_contains  "text" does not appear
  refresh              count refresh events of "kind" (any if omitted) >= "min" / <= "max"
  no_warnings          the twin reported no warnings (e.g. framebuffer overruns)
  warning_contains     a warning containing "text" was reported
  state                get_state()["key"] == "equals"
  hash                 panel hash equals "value"

"scope": "last" restricts serial/refresh checks to output produced by the last
"cmd" step; the default "all" uses everything since boot.
"modes": ["1bit"] (or ["3bit"]) restricts any step to one display mode.

Set FLICKTWIN_UPDATE_GOLDENS=1 to (re)write golden images instead of comparing.
Actual screenshots and diff images are written to FLICKTWIN_ARTIFACTS
(default: twin/artifacts/<scenario>/).
"""
import json
import os
from pathlib import Path
from typing import Any, Dict, Optional, Union

from .client import TWIN_DIR, Twin
from .png import compare, read_png, write_png

GOLDEN_DIR = TWIN_DIR / "tests" / "golden"


class ScenarioError(AssertionError):
    pass


def _artifacts_dir(name: str) -> Path:
    base = Path(os.environ.get("FLICKTWIN_ARTIFACTS", TWIN_DIR / "artifacts"))
    d = base / name
    d.mkdir(parents=True, exist_ok=True)
    return d


def load_scenario(path: Union[str, Path]) -> Dict[str, Any]:
    with open(path) as f:
        sc = json.load(f)
    sc.setdefault("name", Path(path).stem)
    return sc


def run_scenario(scenario: Union[str, Path, Dict[str, Any]], twin: Optional[Twin] = None,
                 grayscale: Optional[bool] = None, log=print) -> Dict[str, Any]:
    """Run a scenario; raises ScenarioError on the first failed expectation.

    Returns a summary dict (steps run, artifacts written)."""
    sc = scenario if isinstance(scenario, dict) else load_scenario(scenario)
    cfg = dict(sc.get("twin", {}))
    gray = cfg.get("grayscale", False) if grayscale is None else grayscale
    mode = "3bit" if gray else "1bit"
    name = sc["name"]
    art = _artifacts_dir("%s-%s" % (name, mode))
    update = os.environ.get("FLICKTWIN_UPDATE_GOLDENS") == "1"
    sd_template = cfg.get("sd_template")
    if sd_template and not os.path.isabs(sd_template):
        sd_template = str((TWIN_DIR / sd_template).resolve())

    own = twin is None
    t = twin or Twin(grayscale=gray, args=cfg.get("args", []), fixtures=cfg.get("fixtures", True),
                     sd_template=sd_template)
    summary = {"name": name, "mode": mode, "steps": 0, "artifacts": str(art)}
    serial_mark, refresh_mark = 0, 0
    try:
        for i, step in enumerate(sc["steps"]):
            label = "%s step %d %s" % (name, i, json.dumps(step))
            if "modes" in step and mode not in step["modes"]:
                continue
            if "cmd" in step:
                args = {k: v for k, v in step.items() if k not in ("cmd", "repeat", "comment", "modes")}
                serial_mark = len(t.serial_log())
                ev = t.refresh_events()
                refresh_mark = ev[-1]["seq"] + 1 if ev else 0
                for _ in range(int(step.get("repeat", 1))):
                    t.call(step["cmd"], **args)
            elif "expect" in step:
                _expect(t, step, label, mode, art, update, serial_mark, refresh_mark, log)
            summary["steps"] += 1
        # Always keep the final screen for inspection.
        t.screenshot(str(art / "final.png"))
        (art / "serial.log").write_text(t.serial_log())
    except Exception:
        try:
            t.screenshot(str(art / "failure.png"))
            (art / "serial.log").write_text(t.serial_log())
        except Exception:
            pass
        raise
    finally:
        if own:
            t.close()
    return summary


def _expect(t: Twin, step, label, mode, art: Path, update, serial_mark, refresh_mark, log):
    kind = step["expect"]
    scope_last = step.get("scope", "all") == "last"
    if kind == "golden":
        golden = GOLDEN_DIR / mode / (step["name"] + ".png")
        actual_path = art / (step["name"] + ".png")
        t.screenshot(str(actual_path), **({"source": step["source"]} if "source" in step else {}))
        if update or not golden.exists():
            if not update:
                raise ScenarioError("%s: golden %s missing (run with FLICKTWIN_UPDATE_GOLDENS=1 to create it; "
                                    "actual image at %s)" % (label, golden, actual_path))
            golden.parent.mkdir(parents=True, exist_ok=True)
            golden.write_bytes(actual_path.read_bytes())
            log("updated golden %s" % golden)
            return
        d = compare(read_png(golden), read_png(actual_path), step.get("threshold", 32))
        tol = step.get("tolerance", 0.001)
        if d.fraction > tol:
            if d.diff_image:
                write_png(art / (step["name"] + ".diff.png"), d.diff_image)
            raise ScenarioError("%s: %d pixels (%.4f%%) differ from %s (tolerance %.4f%%); see %s" % (
                label, d.differing, 100 * d.fraction, golden, 100 * tol, art))
    elif kind in ("serial_contains", "serial_not_contains"):
        text = t.serial_log(serial_mark if scope_last else 0)
        found = step["text"] in text
        if found != (kind == "serial_contains"):
            raise ScenarioError("%s: serial log %s %r" % (
                label, "does not contain" if kind == "serial_contains" else "contains", step["text"]))
    elif kind == "refresh":
        events = t.refresh_events(refresh_mark if scope_last else 0)
        n = sum(1 for e in events if "kind" not in step or e["kind"] == step["kind"])
        if n < step.get("min", 0) or ("max" in step and n > step["max"]):
            raise ScenarioError("%s: %d matching refresh events (%s)" % (label, n, [e["kind"] for e in events]))
    elif kind == "no_warnings":
        w = t.warnings()
        if w:
            raise ScenarioError("%s: twin warnings: %s" % (label, w))
    elif kind == "warning_contains":
        if not any(step["text"] in w for w in t.warnings()):
            raise ScenarioError("%s: no warning containing %r (got %s)" % (label, step["text"], t.warnings()))
    elif kind == "state":
        v = t.get_state().get(step["key"])
        if v != step["equals"]:
            raise ScenarioError("%s: state[%s] = %r" % (label, step["key"], v))
    elif kind == "hash":
        h = t.framebuffer_hash()
        if h != step["value"]:
            raise ScenarioError("%s: panel hash %s != %s" % (label, h, step["value"]))
    else:
        raise ScenarioError("%s: unknown expectation %r" % (label, kind))
