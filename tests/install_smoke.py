"""Exercise the documented install sequence against a local mock clock after a release build."""
from email import policy
from email.parser import BytesParser
import gzip
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import sys
from threading import Thread
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlencode
from urllib.request import HTTPCookieProcessor, ProxyHandler, Request, build_opener


ROOT = Path(__file__).resolve().parents[1]
PASSWORD = "example-smoke-password"


def multipart(name, filename, data):
    boundary = "metro-install-fixture"
    prefix = (f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"; filename="{filename}"\r\n'
              'Content-Type: application/octet-stream\r\n\r\n').encode()
    return prefix + data + f'\r\n--{boundary}--\r\n'.encode(), {
        "Content-Type": f"multipart/form-data; boundary={boundary}"
    }


def exercise(image_path):
    image = image_path.read_bytes()
    version = image_path.stem.removeprefix("SDPro_MetroClock_v")
    assets = {
        "metro-ui.html.gz": (ROOT / "web/metro.html").read_bytes(),
        "metro-network.json.gz": gzip.decompress((ROOT / "data/metro-network.json.gz").read_bytes()),
    }
    system = (ROOT / "web/system.html").read_bytes()
    state = {"version": "SDPro_V1.0.6", "session": 0, "router": False, "assets": {}, "key": ""}

    class Clock(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def reply(self, body=b"", status=200, **headers):
            self.send_response(status)
            for key, value in headers.items():
                self.send_header(key.replace("_", "-"), value)
            self.end_headers()
            self.wfile.write(body)

        def signed_in(self):
            return self.headers.get("Cookie") == f"sc=mock-{state['session']}"

        def upload(self, data):
            header = f"Content-Type: {self.headers['Content-Type']}\r\n\r\n".encode()
            message = BytesParser(policy=policy.default).parsebytes(header + data)
            (part,) = message.iter_parts()
            return part.get_param("name", header="content-disposition"), part.get_filename(), part.get_payload(decode=True)

        def do_GET(self):
            if self.path == "/version":
                self.reply(state["version"].encode())
            elif state["version"] == "SDPro_V1.0.6":
                self.reply(b'<form action="/update_ota">Stock firmware update</form>')
            elif self.path == "/system":
                self.reply(system)
            elif self.path == "/status":
                self.reply(json.dumps({"version": state["version"], "storageReady": True,
                                       "connected": state["router"], "apIp": "192.168.4.1"}).encode())
            elif self.path == "/api/metro":
                if not self.signed_in():
                    self.reply(status=401)
                else:
                    self.reply(json.dumps({"hasApiKey": bool(state["key"])}).encode())
            elif self.path.startswith("/api/metro/resolve?"):
                if not self.signed_in():
                    self.reply(status=401)
                elif not state["key"]:
                    self.reply(status=428)
                else:
                    self.reply(b'{"errorMessage":{"code":"INFO-000"}}')
            elif self.path == "/api/metro/catalog":
                if not self.signed_in():
                    self.reply(status=401)
                else:
                    self.reply(state["assets"]["metro-network.json.gz"], Content_Encoding="gzip")
            elif self.path == "/":
                self.reply(state["assets"]["metro-ui.html.gz"], Content_Encoding="gzip")
            else:
                self.reply(status=404)

        def do_POST(self):
            data = self.rfile.read(int(self.headers["Content-Length"]))
            if self.path == "/update_ota":
                stock = state["version"] == "SDPro_V1.0.6"
                name, filename, payload = self.upload(data)
                if name != "update" or payload != image or (stock and not filename.startswith("SDP")):
                    self.reply(status=400)
                elif not stock and not self.signed_in():
                    self.reply(status=401)
                else:
                    state["version"] = version
                    state["session"] += 1
                    self.reply(b"OK")
            elif self.path == "/api/auth/login":
                if parse_qs(data.decode()) != {"password": [PASSWORD]}:
                    self.reply(status=403)
                else:
                    self.reply(b"OK", Set_Cookie=f"sc=mock-{state['session']}; Path=/; HttpOnly")
            elif not self.signed_in():
                self.reply(status=401)
            elif self.path == "/api/metro/asset":
                name, filename, payload = self.upload(data)
                if name != "file" or filename not in assets or gzip.decompress(payload) != assets[filename]:
                    self.reply(status=400)
                else:
                    state["assets"][filename] = payload
                    self.reply(b"OK")
            elif self.path == "/api/metro/key":
                key = parse_qs(data.decode()).get("apiKey", [""])[0].strip()
                if not key or len(key) > 80:
                    self.reply(status=400)
                else:
                    state["key"] = key
                    self.reply(status=204)
            elif self.path == "/connect":
                form = parse_qs(data.decode())
                if not form.get("ssid") or not form.get("password"):
                    self.reply(status=400)
                else:
                    state["router"] = True
                    state["session"] += 1
                    self.reply(b"Saved. Rebooting...")
            else:
                self.reply(status=404)

    with ThreadingHTTPServer(("127.0.0.1", 0), Clock) as server:
        worker = Thread(target=server.serve_forever, daemon=True)
        worker.start()
        base = f"http://127.0.0.1:{server.server_port}"
        client = build_opener(ProxyHandler({}), HTTPCookieProcessor())

        def request(path, data=None, headers=None):
            with client.open(Request(base + path, data, headers or {}), timeout=10) as response:
                return response.read()

        def sign_in():
            assert request("/api/auth/login", urlencode({"password": PASSWORD}).encode()) == b"OK"

        try:
            assert b"Stock firmware update" in request("/")
            assert request("/version") == b"SDPro_V1.0.6"
            data, headers = multipart("update", image_path.name, image)
            assert request("/update_ota", data, headers) == b"OK"
            status = json.loads(request("/status"))
            assert status["version"] == version and status["storageReady"] and not status["connected"]
            result = subprocess.run([sys.executable, "scripts/metro_assets.py", base], cwd=ROOT,
                                    input=PASSWORD + "\n", text=True, capture_output=True, timeout=30)
            assert result.returncode == 0, result.stderr
            assert "Verified 19 lines" in result.stdout, result.stdout
            assert gzip.decompress(request("/")) == assets["metro-ui.html.gz"]
            assert b'Type="file"'.lower() in request("/system").lower()
            sign_in()
            assert b"Rebooting" in request("/connect", urlencode({"ssid": "Mock Network", "password": "example-router-password"}).encode())
            assert json.loads(request("/status"))["connected"]
            sign_in()
            assert json.loads(request("/api/metro")) == {"hasApiKey": False}
            resolve = "/api/metro/resolve?" + urlencode({"station": "마곡나루"})
            try:
                request(resolve)
            except HTTPError as error:
                assert error.code == 428
                error.close()
            else:
                raise AssertionError("Direction lookup accepted before key registration")
            assert request("/api/metro/key", urlencode({"apiKey": "mock-seoul-api-key"}).encode()) == b""
            assert json.loads(request(resolve))["errorMessage"]["code"] == "INFO-000"
            assert request("/update_ota", data, headers) == b"OK"
            sign_in()
            assert json.loads(request("/status"))["version"] == version
            assert gzip.decompress(request("/api/metro/catalog")) == assets["metro-network.json.gz"]
            assert json.loads(request("/api/metro")) == {"hasApiKey": True}
        finally:
            server.shutdown()
            worker.join()


if __name__ == "__main__":
    release = ROOT / "release"
    expected, filename = (release / "SHA256SUMS.txt").read_text().split()
    image = release / filename
    assert hashlib.sha256(image.read_bytes()).hexdigest() == expected
    assert image.stat().st_size <= 0x7E000 and filename.startswith("SDP")
    exercise(image)
    print("PASS: mock stock OTA, web and catalog install, Wi-Fi configuration, key registration before direction lookup, authenticated Metro OTA")
