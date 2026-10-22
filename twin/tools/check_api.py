#!/usr/bin/env python3
"""API drift check: fail when the firmware starts using hardware/platform APIs
that the twin has not been reviewed to model.

The compiler already rejects APIs that are completely missing from the shims.
This check catches the subtler case: an API that compiles (because it comes from
a real library, an Adafruit_GFX base class, or a permissive stub) but whose
behaviour on the twin has never been checked. Every hardware-facing call and
every #include the firmware uses must be listed in tools/api_manifest.json.

Usage:
    python3 twin/tools/check_api.py [--firmware DIR] [--build DIR] [--update]

  --update   rewrite the manifest with the APIs currently used (review the
             diff, make sure the twin models each new entry, then commit).
"""
import argparse
import json
import re
import sys
from pathlib import Path

TWIN_DIR = Path(__file__).resolve().parent.parent
MANIFEST = TWIN_DIR / "tools" / "api_manifest.json"

# Firmware receiver variable -> API group in the manifest.
RECEIVERS = {
    "display": "Inkplate",
    "inkplate": "Inkplate",
    "touchscreen": "Touchscreen",
    "frontlight": "Frontlight",
    "Serial": "Serial",
    "WiFi": "WiFi",
    "server": "WebServer",
    "dnsServer": "DNSServer",
    "SD": "SdFat",
    "sd": "SdFat",
    "ESP": "ESP",
}
CALL_RE = re.compile(r"\b(%s)\s*(?:->|\.)\s*([A-Za-z_]\w*)\s*\(" % "|".join(RECEIVERS))
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
SOURCE_GLOBS = ("*.cpp", "*.h", "*.ino", "*.c")


def strip_comments(text):
    """Remove // and /* */ comments, keeping string literals intact."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            out.append("\n" * text.count("\n", i, n if j < 0 else j))
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def firmware_sources(fw):
    files = []
    for g in SOURCE_GLOBS:
        files += [p for p in fw.glob(g) if p.is_file()]
    return sorted(files)


def scan(fw):
    calls, includes, where = {}, set(), {}
    for path in firmware_sources(fw):
        text = strip_comments(path.read_text(errors="replace"))
        for m in CALL_RE.finditer(text):
            group = RECEIVERS[m.group(1)]
            calls.setdefault(group, set()).add(m.group(2))
            line = text.count("\n", 0, m.start()) + 1
            where.setdefault((group, m.group(2)), "%s:%d" % (path.name, line))
        for m in INCLUDE_RE.finditer(text):
            name = m.group(2)
            # Firmware-local headers are always available.
            if m.group(1) == '"' and (fw / name).exists():
                continue
            includes.add(name)
            line = text.count("\n", 0, m.start()) + 1
            where.setdefault(("#include", name), "%s:%d" % (path.name, line))
    return {k: sorted(v) for k, v in calls.items()}, sorted(includes), where


def shim_text(build):
    """Concatenated text of everything that can provide an API to the firmware."""
    roots = [TWIN_DIR / "shim", TWIN_DIR / "core", TWIN_DIR / "overlay"]
    if build:
        roots.append(Path(build) / "inkplate")
    chunks = []
    for r in roots:
        if r.exists():
            for p in r.rglob("*"):
                if p.suffix in (".h", ".hpp", ".cpp", ".c") and p.is_file():
                    chunks.append(p.read_text(errors="replace"))
    return "\n".join(chunks)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--firmware", default=str(TWIN_DIR.parent), help="firmware source dir")
    ap.add_argument("--build", default=str(TWIN_DIR / "build"),
                    help="twin build dir (for the staged Inkplate/Adafruit_GFX sources)")
    ap.add_argument("--update", action="store_true", help="rewrite the manifest")
    args = ap.parse_args()

    fw = Path(args.firmware)
    calls, includes, where = scan(fw)
    if args.update:
        MANIFEST.write_text(json.dumps({"calls": calls, "includes": includes}, indent=2) + "\n")
        print("wrote %s" % MANIFEST)
        return 0

    manifest = json.loads(MANIFEST.read_text())
    errors = []
    for group, names in calls.items():
        known = set(manifest["calls"].get(group, []))
        for n in names:
            if n not in known:
                errors.append("new %s API %s() used at %s" % (group, n, where[(group, n)]))
    for inc in includes:
        if inc not in manifest["includes"]:
            errors.append("new #include <%s> at %s" % (inc, where[("#include", inc)]))

    # Every reviewed API must still be provided somewhere in the twin.
    build = Path(args.build) if Path(args.build).exists() else None
    provided = shim_text(build)
    for group, names in manifest["calls"].items():
        for n in names:
            if not re.search(r"\b%s\s*\(" % re.escape(n), provided):
                if build or group != "Inkplate":
                    errors.append("manifest lists %s.%s() but no shim provides it" % (group, n))

    unused = [(g, n) for g, names in manifest["calls"].items() for n in names if n not in calls.get(g, [])]
    if unused:
        print("note: manifest entries no longer used by the firmware: %s" %
              ", ".join("%s.%s" % u for u in unused))
    total = sum(len(v) for v in calls.values())
    if errors:
        print("API drift detected (%d issue(s)):" % len(errors))
        for e in errors:
            print("  - " + e)
        print("\nMake the twin model the new API (shim/, core/), add a scenario that exercises it,\n"
              "then run: python3 twin/tools/check_api.py --update  and commit the manifest.")
        return 1
    print("API check OK: %d hardware calls in %d groups, %d external includes covered."
          % (total, len(calls), len(includes)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
