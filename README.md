# RotorHazard Race Counter

A battery-friendly ePaper sign that shows the current **heat** and **race number** at the flight line, so
pilots and spectators can always tell which race is up. Designed and built by RocketSled.

The display is a Seeed XIAO ESP32-C3 driving a 5.83" 648×480 monochrome ePaper panel in a 3D-printed
housing with two push-buttons (DN / UP). It can be run standalone from the buttons, or over WiFi from a
small built-in web page and HTTP API. The longer-term goal of this project is to have it follow a
[RotorHazard](https://github.com/RotorHazard/RotorHazard) timer automatically.

## Repository layout

| Path | Contents |
|---|---|
| `firmware/RaceCounter/` | Arduino sketch (RocketSled's V2.0 of Aug 9 2026, plus the fixes listed in `git log`) |
| `firmware/RaceCounter/sketch.yaml` | Board / partition / option settings, read by arduino-cli and Arduino IDE 2.2+ |
| `firmware/RaceCounter/bg_png.h` | Web-page background image (RotorHazard logo) embedded as a byte array; regenerate with `tools/bin2header.py` |
| `firmware/RaceCounter/driver.h` | Seeed_GFX hardware selection (board + panel). **Required** – see Building |
| `firmware/libraries/Seeed_GFX/` | Git submodule: the display library, pinned to a known-good commit |
| `docs/parts-list.md` | Bill of materials |
| `assets/` | Source font (Orbitron Bold, OFL) and logo images used to generate the `.h` bitmaps |
| `tools/bin2header.py` | Turns a binary file into a `PROGMEM` C array header |

## Hardware

* Seeed Studio **XIAO ESP32-C3**
* Seeed **ePaper Driver Board for XIAO** (V2) – 24-pin FPC, JST battery input with charge IC
* **5.83" monochrome ePaper**, 648×480, UC8179 controller, 24-pin FPC
* Two momentary push-buttons to GND: **DN → D5**, **UP → D6** (internal pull-ups)
* 18650 Li-ion cell with a latching power button; USB-C pass-through port for charging and programming
* 3D-printed housing (STLs not yet in this repo)

Full list with links: [docs/parts-list.md](docs/parts-list.md).

## Building the firmware

RocketSled builds with Visual Studio + VisualMicro; the sketch also builds in the Arduino IDE 2.x and with
`arduino-cli`. Verified 2026-09-20 with `arduino-cli` 0.35.2, esp32 core 3.3.8 and Seeed_GFX 2.0.3
(commit `0dfdd71`):

```
Sketch uses 1251067 bytes (39%) of program storage space. Maximum is 3145728 bytes.
Global variables use 37920 bytes (11%) of dynamic memory.
```

The board and options are recorded in `firmware/RaceCounter/sketch.yaml`, which arduino-cli and Arduino
IDE 2.2+ pick up automatically, so the command line is just:

```bash
arduino-cli compile --libraries firmware/libraries firmware/RaceCounter
```

1. **Board package:** *esp32 by Espressif Systems* (RocketSled used 3.3.8; board-manager URL
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`). Board: **XIAO_ESP32C3**.
   * Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)** – the sketch filled 95 % of the default
     scheme's 1.2 MB app slot, and the device doesn't use OTA, so the larger no-OTA layout costs nothing.
     (RocketSled's units were flashed with the *Default 4MB* scheme; re-flashing with this one is fine – the
     partition table is rewritten as part of the upload.)
   * USB CDC On Boot: **Enabled**
2. **Display library:** **[Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX)** is not in the Arduino
   Library Manager, so it is vendored here as a git submodule at `firmware/libraries/Seeed_GFX`, pinned
   to a commit known to build. Get it with
   ```bash
   git submodule update --init
   ```
   (or clone with `--recurse-submodules`). Then make it visible to the Arduino IDE by linking or copying it
   into your sketchbook's `libraries` folder – on Windows a directory junction avoids a second copy:
   ```
   mklink /J "<sketchbook>\libraries\Seeed_GFX" "<repo>\firmware\libraries\Seeed_GFX"
   ```
   `arduino-cli` users can skip the link and pass `--libraries firmware/libraries` instead.

   Seeed_GFX is a fork of TFT_eSPI and **conflicts with TFT_eSPI / Adafruit_GFX – remove those from your
   `libraries` folder first.** The sketch will still compile against the wrong library, it just won't drive
   the panel.
3. **`driver.h`** must be in the sketch folder (it is). Seeed_GFX picks it up automatically:
   ```c
   #define BOARD_SCREEN_COMBO 503   // 5.83 inch monochrome ePaper (UC8179), 648x480
   #define USE_XIAO_EPAPER_DRIVER_BOARD
   ```
   If you change the panel or carrier board, regenerate this with Seeed's configuration tool.
4. Compile and upload the sketch over USB-C. That's it – there is no separate filesystem image to upload;
   the web page's background image is compiled into the firmware (`bg_png.h`).

All other libraries (`WiFi`, `WebServer`, `Preferences`) ship with the esp32 core.

## Operation

### First power-up / WiFi setup

* With no saved credentials the device starts an open access point **`RaceCounter-Setup`** and shows the AP
  IP (`192.168.4.1`) and MAC on the panel. Connect to it, open `http://192.168.4.1`, enter your SSID and
  password, *Save & Reboot*.
* It then connects to that network, shows the IP it was given and its MAC for a few seconds, and displays
  the RotorHazard splash screen. The web interface is at `http://<ip>/`.
* The MAC shown on both the AP screen and the "Connected" screen is the station MAC – the one your router
  sees – so it can be used directly for a static DHCP lease.

### Buttons

| Action | Result | Timing |
|---|---|---|
| UP | race number +1 (1…99, wraps) | Acts the moment the button is released, or after 0.25 s if it is held. One press = one step; holding does not repeat. |
| DN | race number −1 (1…99, wraps) | Same as UP. Does nothing while the splash screen is showing. |
| UP + DN together, in normal operation | toggle **PRACTICE** mode | Acts as soon as both are down; the second button must go down within 0.25 s of the first. Holding them does nothing further – release both before the next press. |
| UP + DN held at power-on | erase saved WiFi credentials and standalone flag, restart into AP mode | The pins are read **once**, about 2–3 s after power-on. Hold both before switching on, keep holding until "Old SSID and password deleted…" appears, then release. |
| UP + DN while "Connecting to SSID…" | same – use this if it is stuck on a network it can't reach | Checked every 0.3 s while connecting; any simultaneous press of ≥ 0.3 s works. |
| UP + DN while in AP mode | enable **standalone mode** (no WiFi, buttons only) and restart | Same both-down detection as the practice toggle. To leave standalone mode, use the power-on hold. |

Heat number can only be set from the web interface / HTTP API.

### Web interface

The main page has −/+ and *Set* controls for Heat and Race, a *Practice* toggle, and *Reset*. The 🛠 icon
top-right opens the settings page where the SSID/password can be changed (*Update*) or erased (*Clear*);
both reboot the device. The settings page also shows the firmware version (`FW_VERSION` in `RaceCounter.ino`,
also printed in the serial banner at boot).

### HTTP API

Every endpoint except `/reset` is a plain `GET`, changes the display, and replies `303 → /` (so a browser
lands back on the main page). Scripts should send `allow_redirects=False` or just ignore the redirect.

| Endpoint | Effect |
|---|---|
| `/heatInc`, `/heatDec` | heat ±1 (0…99, wraps); race resets to 1 |
| `/heatSet?v1=N` | set heat to N (0–99). Heat 0 hides the heat number; banner shows "RACE #". Nonzero heat shows "Heat N" and resets race to 1 |
| `/raceInc`, `/raceDec` | race ±1 (1…99, wraps) |
| `/raceSet?v2=N` | set race to N (1–99) |
| `/practice` | **toggle** practice mode (banner "PRACTICE", big "P") |
| `/splash` | heat 0, race 0, practice off, show the RotorHazard splash screen (this is the web *Reset* button) |
| `/settings` | WiFi settings page |
| `/reset` (`POST` only) | **erases the saved WiFi credentials** and reboots into AP mode (the settings-page *Clear* button, behind a confirmation) |
| `/bg.png` | the web page background image |

Out-of-range values are ignored. There is no endpoint to *read* the current state.

## Known issues / roadmap

The sketch is RocketSled's V2.0 plus a series of small fixes, each its own commit – see `git log`. The
first import commit holds the code exactly as received. Note for anyone building from that original: the
Arduino build system matches include names to libraries case-sensitively even on Windows, so
`<webserver.h>` / `<preferences.h>` had to become `WebServer.h` / `Preferences.h` (VisualMicro is more
forgiving).

Remaining items:

* The WiFi connect loop never times out (the only escape is holding both buttons).
* API is browser-oriented: numeric heat only (RotorHazard heats have names), practice is toggle-only, no
  status endpoint. Needed before RotorHazard can drive it.
* Planned: connect to a RotorHazard server directly (Socket.IO `race_status` / `current_heat` events carry
  the heat id and `next_round`; `/api/heat/<id>` gives the name) so the display follows the timer with no
  plugin required.

## Credits

Hardware, housing and firmware by RocketSled. RotorHazard logo © the RotorHazard project.
Orbitron font by Matt McInerney, SIL Open Font License.
