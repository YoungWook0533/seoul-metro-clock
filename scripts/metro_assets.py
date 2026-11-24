"""Install the Metro settings page and station catalog on a clock over its LAN."""
import argparse
import getpass
import gzip
import json
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlsplit
from urllib.request import HTTPCookieProcessor, ProxyHandler, Request, build_opener
from uuid import uuid4


def upload_asset(opener, base, name, data):
    boundary = uuid4().hex
    header = (f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="{name}"\r\n'
              'Content-Type: application/octet-stream\r\n\r\n').encode()
    request = Request(base + '/api/metro/asset', header + data + f'\r\n--{boundary}--\r\n'.encode(),
                      {'Content-Type': f'multipart/form-data; boundary={boundary}'})
    with opener.open(request, timeout=45) as response:
        if response.read().strip() != b'OK':
            raise RuntimeError(f'The clock did not accept {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('address', help='Clock URL, for example http://192.168.4.1')
    args = parser.parse_args()
    base = args.address.rstrip('/')
    address = urlsplit(base)
    if address.scheme not in ('http', 'https') or not address.hostname or address.username or address.path or address.query or address.fragment:
        parser.error('Enter the clock origin as http://IP or http://hostname.local')
    root = Path(__file__).resolve().parents[1]
    page = gzip.compress((root / 'web/metro.html').read_bytes(), mtime=0)
    catalog = (root / 'data/metro-network.json.gz').read_bytes()
    opener = build_opener(ProxyHandler({}), HTTPCookieProcessor())
    password = getpass.getpass('Clock password: ')
    with opener.open(base + '/api/auth/login', urlencode({'password': password}).encode(), timeout=15):
        pass
    for name, asset in (('metro-ui.html.gz', page), ('metro-network.json.gz', catalog)):
        upload_asset(opener, base, name, asset)
        print(f'Installed {name} ({len(asset)} bytes)')
    with opener.open(base + '/api/metro/catalog', timeout=15) as response:
        payload = response.read()
        if response.headers.get('Content-Encoding') == 'gzip':
            payload = gzip.decompress(payload)
        print(f"Verified {len(json.loads(payload)['lines'])} lines on the clock.")


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, HTTPError, URLError) as error:
        raise SystemExit(f'Asset installation failed: {error}') from None
