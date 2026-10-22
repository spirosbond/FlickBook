import os
import socket
import sys
from pathlib import Path

import pytest

TWIN_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TWIN_DIR / "python"))

from flicktwin import Twin, TwinError, default_binary  # noqa: E402


def _binary(gray):
    try:
        return default_binary(gray)
    except TwinError:
        return None


MODES = [pytest.param(False, id="1bit")]
if _binary(True):
    MODES.append(pytest.param(True, id="3bit"))


def pytest_report_header(config):
    return "flicktwin: %s | flicktwin_gray: %s" % (_binary(False), _binary(True))


@pytest.fixture(scope="session", autouse=True)
def _require_binary():
    if not _binary(False):
        pytest.exit("flicktwin binary not found: build twin/ first or set FLICKTWIN_BIN", returncode=2)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@pytest.fixture
def twin():
    t = Twin(args=["--web-port", str(free_port())])
    yield t
    t.close()
