"""Command line entry point.

    python -m flicktwin run SCENARIO.json [...] [--gray]   run scenarios (exit 1 on failure)
    python -m flicktwin mcp [--gray] [-- FLICKTWIN_ARGS]   MCP server on stdio
"""
import argparse
import sys

from .mcp_server import Server
from .scenario import ScenarioError, run_scenario


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    extra = []
    if "--" in argv:
        i = argv.index("--")
        argv, extra = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser(prog="flicktwin")
    sub = ap.add_subparsers(dest="command", required=True)
    r = sub.add_parser("run", help="run scenario files")
    r.add_argument("scenarios", nargs="+")
    r.add_argument("--gray", action="store_true", help="use the GRAYSCALE=1 build")
    m = sub.add_parser("mcp", help="serve the twin as MCP tools on stdio")
    m.add_argument("--gray", action="store_true")
    a = ap.parse_args(argv)
    if a.command == "mcp":
        Server(grayscale=a.gray, args=extra).serve()
        return 0
    failed = 0
    for path in a.scenarios:
        try:
            s = run_scenario(path, grayscale=a.gray if a.gray else None, log=lambda m: print(m, file=sys.stderr))
            print("PASS %s (%s, %d steps) -> %s" % (s["name"], s["mode"], s["steps"], s["artifacts"]))
        except ScenarioError as e:
            failed += 1
            print("FAIL %s" % e)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
