# Tests

Run these from the repository root after building once so the TFT library is available.

```sh
node tests/metro_network.js
python tests/metro_geometry.py
python tests/run_safety_tests.py
c++ -std=c++17 -Iinclude tests/metro_estimate.cpp -o /tmp/metro_estimate
/tmp/metro_estimate
```

For host display rendering, install Pillow and use a compiler such as GCC or Clang. You can set `CXX="zig c++"` to use Zig instead. Images are written to `.pio/metro-preview/`.

```sh
python -m pip install Pillow
python tests/run_metro_preview.py
```

To exercise the install order locally, build the release and run the mock clock test:

```sh
python scripts/make_release.py
python tests/install_smoke.py
```

The test uses loopback HTTP and a simulated reboot. It does not update a physical clock.
