# Metro Clock

[한국어 README](README.ko.md) · [Firmware installation](#firmware-installation) · [Button and enclosure](#build-the-button-and-enclosure)

Metro Clock turns an **ESP8266 SD Pro Smart Weather Clock** into a desk display for Seoul-area train arrivals. It highlights the first train you can reach based on your walking time.

<p align="center">
  <img src="docs/images/metro-clock-cover.jpg" width="255" alt="Assembled white Metro Clock showing train arrivals">
  <img src="docs/images/metro-clock-button-demo.gif" width="193" alt="Pressing the top button to cycle through saved stations">
</p>

## Features

- Seoul-area station picker with 19 lines, live arrival estimates, and Seoul weather
- Five favorites that store the line, station, direction, approach route, walking time, and brightness
- An optional GPIO4 button that cycles through occupied favorites in slot order
- **한국어** by default, with **English** available for the Metro settings page and clock display
- Firmware updates and Wi-Fi setup from the clock's web interface

## Settings page

Open the address shown on the clock and sign in with its device password. To follow the English screenshots, select **English** under **표시 언어** (Display language). After [setting up your Seoul subway API key](#register-your-seoul-subway-api-key), choose a line, station, and direction and select **Save and apply**. To keep that setup, choose a slot under **Favorites** and select **Save current settings**.

![Metro Clock web settings showing the current favorite, line, station, and direction with sample data](docs/images/metro-settings-en.png)

Use **Wi-Fi, password, and firmware** at the bottom of the page to open `/system`.

## Hardware

| Supported device | Hardware | Tested starting firmware |
| --- | --- | --- |
| SD Pro Smart Weather Clock | ESP-12F / ESP8266, 4 MB flash, 240 × 240 ST7789 | `SDPro_V1.0.6` |

See the [SD Pro hardware notes](docs/specifications/hardware.md) to identify your board. For the optional button build, gather the following parts.

| Part | What you need |
| --- | --- |
| Power | USB-C |
| Switch | HB05 tactile switch, **6 × 6 × 5 mm**, **4 through-hole pins (DIP-4)**, normally open momentary |
| Wiring | Two thin insulated wires, soldering iron, and multimeter |
| Printed parts | Replacement outer shell and optional fit gauges. Reuse the original inner tray, rear panel, and screws. |

## Firmware installation

Clone or download this repository. The firmware image is built from source. Complete the build while your computer has internet access.

### Option 1. Install with an agent (recommended)

Open this repository in Codex or Claude and ask: “Read [`docs/INSTALL_AGENT.md`](docs/INSTALL_AGENT.md) and follow it to install Metro Clock on my SD Pro.”

### Option 2. Install manually

1. **Build the firmware.** With Python 3 and pip installed, run these commands from the repository root. Use `python3` if needed, and activate a virtual environment if your system restricts pip installs.

   ```sh
   python -m pip install -r requirements-build.txt
   python scripts/make_release.py
   ```

   Use the generated `release/SDPro_MetroClock_*.bin`. Its SHA-256 is in `release/SHA256SUMS.txt`.
2. **Upload the firmware.** Before replacing stock firmware, make a full flash backup if you want to restore it later. For a first install, open your stock SD Pro's current IP and use its update page. Keep the `SDP` filename prefix. To update Metro Clock, sign in at the clock's current address under `/system`, then use **Firmware update**. Keep USB power connected through reboot. [Backup and recovery](docs/MIGRATION.md)
3. **Install the web page and station catalog.** After reboot, join **Smart Weather Clock** with the password shown on the display. Use the same password when the script asks. If the clock is on your router, substitute its LAN IP.

   ```sh
   python scripts/metro_assets.py http://192.168.4.1
   ```

4. **Connect the clock to Wi-Fi.** For first-time Wi-Fi setup, sign in at `http://192.168.4.1/system`, enter your router details, then select **Save and restart**.
5. **[Register your API key](#register-your-seoul-subway-api-key)**, then choose a line, station, and direction and click **Save and apply**.

### Register your Seoul subway API key

1. Sign in to Seoul Open Data. On the [realtime arrivals dataset page](https://data.seoul.go.kr/dataList/OA-12764/F/1/datasetView.do), open **Open API → 인증키 신청 (지하철)** and request a personal key.
2. Open the LAN IP shown on the clock and sign in. Under **Data access**, paste the key and click **Save API key**. Wait for **API key saved** before choosing a station and direction.

   ![Metro Clock settings showing the API key input, Save API key button, and saved confirmation](docs/images/api-key-en.png)

The Seoul Open Data lines use the saved key. Airport Railroad (AREX) uses the operator's feed.

## Build the button and enclosure

### Wire and test the button

![Wiring diagram showing GPIO4 through a normally open HB05 switch to ground](docs/images/button-wiring.png)

1. Unplug USB. Solder one wire to **GPIO4** on the ESP-12F module and the other to **GND**. In the [board photo](docs/specifications/board-esp12f.jpg), the copper antenna is on the right and GPIO4 is the fourth pad from the right on the lower row of eight pads. Verify the pad and ground on your board before soldering. [ESP-12F pinout](https://docs.ai-thinker.com/_media/esp8266/docs/esp-12f_product_specification_en.pdf)
2. Use continuity mode to find two button legs that connect **only while pressed**. Wire those legs between GPIO4 and GND.
3. With USB still unplugged, check for solder bridges and confirm that GPIO4 connects to GND only when the button is pressed. Power on and save at least two favorites to test cycling.

![Actual Metro Clock board with two wires running to the test button on a breadboard](docs/images/button-breadboard.jpg)

### Assemble the printed shell

[Shell STL](hardware/enclosure/outer-shell-v3.stl) · [HB05 retainer STL](hardware/enclosure/hb05-slide-retainer-v3.stl) · [CAD, fit gauges, and print orientation](hardware/enclosure/README.md)

1. **Fit the button into the shell.** Insert the wired switch from inside, then slide the v3 retainer underneath it from the rear. Leave slack in the leads and check that the button returns freely.

   <img src="docs/images/button-inside-shell.jpg" width="400" alt="Wired button installed inside the printed shell">

2. **Slide in the original tray.** Feed it in from the rear with the display facing the bezel. Guide the wires clear of the tray edges and the display ribbon.

   <img src="docs/images/tray-and-shell.jpg" width="400" alt="Original electronics tray aligned with the rear of the printed shell">

3. **Install both underside screws.** Start them loosely, position the tray so the bezel clears the LCD, then tighten gently. Connect USB-C and press the button to check that it advances through your favorites.

   <img src="docs/images/case-screws-and-usb.jpg" width="400" alt="Assembled clock with both underside screws installed and USB-C connected">

## Recovery

If the clock gets stuck at startup, power it on for less than 10 seconds twice, then power it on a third time and leave it running. When the screen shows **Recovery**, join the open **Smart Weather Clock** Wi-Fi, open `http://192.168.4.1/system`, and upload a known-good build. Restoring stock firmware requires a full flash backup and a UART connection. [Migration and recovery guide](docs/MIGRATION.md)

## Known limitations

- Train positions are estimates and arrival times depend on the rail data feed.
- The clock serves its settings over local HTTP. Use a trusted network.

## Project documents

| Topic | Location |
| --- | --- |
| Firmware and web UI | [`src/`](src/) · [`web/`](web/) |
| Build and verification | [Build the firmware](#option-2-install-manually) · [Tests](tests/README.md) · [Changelog](CHANGELOG.md) |
| Data sources | [Seoul Open Data](https://data.seoul.go.kr/dataList/OA-12764/F/1/datasetView.do) · [Airport Railroad](https://www.airportrailroad.com/train/normal/live) · [Open-Meteo](https://open-meteo.com/) · [Polling notes](data/AREX-source-notes.md) |

## License

[GPL-3.0](LICENSE). Derived from [SmartClock-OSS](https://github.com/michaeldale/smartclock-oss). Third-party data and service terms apply to their respective sources.
