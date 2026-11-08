"""The firmware must only use hardware/platform APIs the twin has been reviewed to model."""
import subprocess
import sys

from conftest import TWIN_DIR


def test_firmware_api_matches_manifest():
    r = subprocess.run([sys.executable, str(TWIN_DIR / "tools" / "check_api.py")],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
