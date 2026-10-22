"""The firmware's WebServer runs on a real localhost port: enable it from the settings UI
and use the upload page's API like a browser would."""
import json
import urllib.request
import uuid
from pathlib import Path

from conftest import free_port

from flicktwin import Twin


def _req(url, data=None, method=None, headers=None):
    r = urllib.request.Request(url, data=data, method=method, headers=headers or {})
    with urllib.request.urlopen(r, timeout=10) as resp:
        return resp.status, resp.read()


def test_upload_list_download_delete():
    port = free_port()
    with Twin(args=["--web-port", str(port)]) as t:
        t.tap(662, 30)   # settings
        t.tap(300, 150)  # cloud tab
        t.tap(670, 250)  # Webserver toggle
        assert t.get_state()["wifi_ap"]
        base = "http://127.0.0.1:%d" % port

        status, body = _req(base + "/")
        assert status == 200 and b"<html" in body.lower()

        boundary = uuid.uuid4().hex
        payload = (("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"hello.txt\"\r\n"
                    "Content-Type: text/plain\r\n\r\nhello from the twin\r\n--%s--\r\n") % (boundary, boundary)).encode()
        status, body = _req(base + "/api/upload?path=/books", payload, "POST",
                            {"Content-Type": "multipart/form-data; boundary=" + boundary})
        assert status == 200, body

        status, body = _req(base + "/api/list?path=/books")
        names = [e["name"] for e in json.loads(body)]
        assert "hello.txt" in names

        sd = Path(t.get_state()["sd_root"])
        assert (sd / "books" / "hello.txt").read_text() == "hello from the twin"

        status, body = _req(base + "/api/download?path=/books/hello.txt")
        assert body == b"hello from the twin"

        status, _ = _req(base + "/api/delete?path=/books/hello.txt", method="DELETE")
        assert status == 200
        assert not (sd / "books" / "hello.txt").exists()
