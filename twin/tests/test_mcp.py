"""The MCP wrapper speaks JSON-RPC over stdio and exposes the agent commands as tools."""
import json
import os
import subprocess
import sys

from conftest import TWIN_DIR


def test_mcp_tools_roundtrip():
    env = dict(os.environ, PYTHONPATH=str(TWIN_DIR / "python"))
    p = subprocess.Popen([sys.executable, "-m", "flicktwin", "mcp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         text=True, env=env)

    def rpc(i, method, params=None):
        p.stdin.write(json.dumps({"jsonrpc": "2.0", "id": i, "method": method, "params": params or {}}) + "\n")
        p.stdin.flush()
        return json.loads(p.stdout.readline())

    try:
        assert rpc(1, "initialize", {"protocolVersion": "2024-11-05"})["result"]["serverInfo"]["name"]
        p.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n")
        tools = {t["name"] for t in rpc(2, "tools/list")["result"]["tools"]}
        assert {"tap", "swipe", "screenshot", "get_serial_log", "reset", "rebuild"} <= tools
        res = rpc(3, "tools/call", {"name": "screenshot", "arguments": {}})["result"]
        assert res["content"][0]["type"] == "image" and not res["isError"]
        res = rpc(4, "tools/call", {"name": "tap", "arguments": {"x": 630, "y": 250}})["result"]
        assert json.loads(res["content"][0]["text"])["ok"]
        res = rpc(5, "tools/call", {"name": "tap", "arguments": {}})["result"]
        assert res["isError"]
    finally:
        p.stdin.close()
        p.wait(timeout=20)
